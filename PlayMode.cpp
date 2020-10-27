#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Load.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

//from the game2 base code:
GLuint game2city_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > game2city_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("game2-city.pnct"));
	game2city_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > game2city_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("game2-city.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name) {

		Mesh const &mesh = game2city_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = game2city_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});
//end of code from game2 base code

PlayMode::PlayMode(Client &client_) : client(client_), scene(*game2city_scene) {
	//get pointers to the roofs for convience
	for (auto &transform : scene.transforms) {
		//referenced for checking is a string includes a substring: https://stackoverflow.com/questions/2340281/check-if-a-string-contains-a-string-in-c
		if (transform.name.find("Roof") != std::string::npos)
			roof_transforms.emplace_back(&transform);
	}
	
	//get pointers to cameras for convenience:
	if (scene.cameras.size() != 4) throw std::runtime_error("Expecting scene to have exactly four cameras, but it has " + std::to_string(scene.cameras.size()));
	for (auto cam : scene.cameras)
		cameras.emplace_back(cam);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		}
		//from game the game 3 base code:
		else if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		}
		//end of code from the game 3 base code:
		else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	}
	//from my game3 code (edited from base code):
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			//this section below lets the camera move based on mouse motion, but never about the y-axis
			camera->transform->rotation
				*= glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				*= glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f));
			glm::vec3 euler = quaternion_to_euler(camera->transform->rotation);
			euler.x = glm::min(euler.x, 150.0f);
			euler.x = glm::max(euler.x, 30.0f);
			euler.y = 0.0f;
			camera->transform->rotation = euler_to_quaternion(euler);
			camera->transform->rotation = glm::normalize(camera->transform->rotation);
			return true;
		}
	}
	//end of code from game3

	return false;
}

void PlayMode::update(float elapsed) {
	//check if the player is it:
	if (server_message == "You are it! Tag someone!")
		time_it += elapsed;
	else if (time_it != 5000.0f)
		time_it = 5000.0f;

	//use the fact that camera != nullptr as an indication that the camera is connected
	if (camera != nullptr) {
		time_since_connection += elapsed;

		if (time_since_connection >= 0.2f) {
			//queue camera position for sending to server (a ten byte message of type 'b'):
			client.connections.back().send('b');

			//get the sign for each axis of the camera position:
			uint8_t x_pos_sign = 0;
			if (camera->transform->position.x < 0.0f) x_pos_sign = 1;
			uint8_t y_pos_sign = 0;
			if (camera->transform->position.y < 0.0f) y_pos_sign = 1;
			uint8_t z_pos_sign = 0;
			if (camera->transform->position.z < 0.0f) z_pos_sign = 1;

			//get the digits to the left of the deimcal point:
			uint8_t x_pos = absolute_float_to_uint8_t(camera->transform->position.x);
			uint8_t y_pos = absolute_float_to_uint8_t(camera->transform->position.y);
			uint8_t z_pos = absolute_float_to_uint8_t(camera->transform->position.z);

			//get the first two digits to the right of the decimal point:
			uint8_t x_pos_decimal = absolute_float_to_uint8_t((std::abs(camera->transform->position.x) - x_pos) * 100.0f);
			uint8_t y_pos_decimal = absolute_float_to_uint8_t((std::abs(camera->transform->position.y) - y_pos) * 100.0f);
			uint8_t z_pos_decimal = absolute_float_to_uint8_t((std::abs(camera->transform->position.z) - z_pos) * 100.0f);

			//queue the current camera position:
			client.connections.back().send(x_pos_sign);
			client.connections.back().send(x_pos);
			client.connections.back().send(x_pos_decimal);
			client.connections.back().send(y_pos_sign);
			client.connections.back().send(y_pos);
			client.connections.back().send(y_pos_decimal);
			client.connections.back().send(z_pos_sign);
			client.connections.back().send(z_pos);
			client.connections.back().send(z_pos_decimal);

			//TODO: queue the current camera rotation in euler angles
		}

		//move camera:
		if (time_it >= 3.0f) {
			//from my game3 code (edited from base code), edited for game6:
			//how fast the player moves
			constexpr float PlayerSpeed = 3.0f;

			//combine inputs into a move:
			glm::vec2 move = glm::vec2(0.0f);
			if (left.pressed && !right.pressed) move.x = -1.0f;
			if (!left.pressed && right.pressed) move.x = 1.0f;
			if (down.pressed && !up.pressed) move.y = -1.0f;
			if (!down.pressed && up.pressed) move.y = 1.0f;

			//make it so that moving diagonally doesn't go faster:
			if (move != glm::vec2(0.0f)) {
				if (time_it == 5000.0f)
					move = glm::normalize(move) * PlayerSpeed * elapsed;
				else
					move = glm::normalize(move) * PlayerSpeed * elapsed * 1.5f;
			}

			glm::mat4x3 frame = camera->transform->make_local_to_world();
			glm::vec3 right = frame[0];
			glm::vec3 forward = -frame[2];

			glm::vec3 new_pos = camera->transform->position + move.x * right + move.y * forward;

			//check if camera would enter a building
			for (auto transform : roof_transforms) {
				glm::vec3 roof_pos = transform->position;
				glm::vec3 roof_scale = transform->scale;
				if (new_pos.x >= roof_pos.x - roof_scale.x - 0.125f && new_pos.x <= roof_pos.x + roof_scale.x + 0.125f &&
					new_pos.y >= roof_pos.y - roof_scale.y - 0.125f && new_pos.y <= roof_pos.y + roof_scale.y + 0.125f) {
					new_pos = camera->transform->position;
					break;
				}
			}

			camera->transform->position = new_pos;
			
			//adjust camera to be within bounds
			if (camera->transform->position.x > 19.0f) camera->transform->position.x = 19.0f;
			else if (camera->transform->position.x < -7.0f) camera->transform->position.x = -7.0f;
			if (camera->transform->position.y > 21.0f) camera->transform->position.y = 21.0f;
			else if (camera->transform->position.y < -15.0f) camera->transform->position.y = -15.0f;
			camera->transform->position.z = 0.6f;
			//end of code from game3

			//tag someone if close enough:
			if (time_it != 5000.0f) {
				for (int i = 0; i < cameras.size(); i++) {
					Scene::Camera *vector_cam = &cameras[i];
					if (camera != vector_cam) {
						glm::vec3 vector_cam_pos = vector_cam->transform->position;
						if (vector_cam_pos.z >= 0.6f) {
							glm::vec3 cam_pos = camera->transform->position;
							float dist = std::sqrt(std::pow(cam_pos.x - vector_cam_pos.x, 2) + std::pow(cam_pos.y - vector_cam_pos.y, 2));
							if (dist <= 0.25f) {
								client.connections.back().send('t');
								client.connections.back().send((uint8_t)(i));
							}
						}
					}
				}
			}
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
			//expecting message(s) like 'm' + 3-byte length + length bytes of text or as a list of floats representing transforms:
			while (c->recv_buffer.size() >= 4) {
				//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
				char type = c->recv_buffer[0];
				if (!(type == 'm' || type == 't')) {
					throw std::runtime_error("Server sent unknown message type '" + std::to_string(type) + "'");
				}

				//the server sent a message to the client:
				if (type == 'm')
				{
					uint32_t size = (
						(uint32_t(c->recv_buffer[1]) << 16) | (uint32_t(c->recv_buffer[2]) << 8) | (uint32_t(c->recv_buffer[3]))
						);
					if (c->recv_buffer.size() < 4 + size) break; //if whole message isn't here, can't process
					//whole message *is* here, so set current server message:
					server_message = std::string(c->recv_buffer.begin() + 4, c->recv_buffer.begin() + 4 + size);

					//and consume this part of the buffer:
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4 + size);

					//for when the player is it:
					if (time_it == 5000.0f && server_message == "You are it! Tag someone!")
						time_it = 0.0f;

					//for help with finding if a string contains a substring: https://stackoverflow.com/questions/2340281/check-if-a-string-contains-a-string-in-c
					if (camera == nullptr && server_message.find("Client is player ") != std::string::npos) {
						//for help with getting an int from a string: https://stackoverflow.com/questions/4442658/c-parse-int-from-string
						std::cout << server_message << std::endl;
						if (server_message == "Client is player 1")
							client_num = 1;
						else if (server_message == "Client is player 2")
							client_num = 2;
						else if (server_message == "Client is player 3")
							client_num = 3;
						else if (server_message == "Client is player 4")
							client_num = 4;
						if (client_num == -1) throw std::runtime_error("Maximum number of players already connected!");
						std::cout << "client_num == " << client_num << std::endl;
						for (int i = 0; i < cameras.size(); i++) {
							Scene::Camera *vector_cam = &cameras[i];
							if (i != (client_num - 1)) {
								if (i == 0)
									vector_cam->transform->position = glm::vec3(19.0f, -15.0f, 0.0f);
								else if (i == 1)
									vector_cam->transform->position = glm::vec3(19.0f, 21.0f, 0.0f);
								else if (i == 2)
									vector_cam->transform->position = glm::vec3(-7.0f, 21.0f, 0.0f);
								else if (i == 3)
									vector_cam->transform->position = glm::vec3(-7.0f, -15.0f, 0.0f);
							}
						}
						camera = &cameras[client_num - 1];
					}
				}

				//the server sent transforms to the client:
				if (type == 't') {
					//for getting a float out of a string: https://www.programiz.com/cpp-programming/string-float-conversion
					//construct position floats
					float x_pos = std::stof(std::to_string(c->recv_buffer[2]));
					x_pos += (std::stof(std::to_string(c->recv_buffer[3])) / 100.0f);
					if (c->recv_buffer[1] == (uint8_t)1) x_pos *= -1.0f;
					float y_pos = std::stof(std::to_string(c->recv_buffer[5]));
					y_pos += (std::stof(std::to_string(c->recv_buffer[6])) / 100.0f);
					if (c->recv_buffer[4] == (uint8_t)1) y_pos *= -1.0f;
					float z_pos = std::stof(std::to_string(c->recv_buffer[8]));
					z_pos += (std::stof(std::to_string(c->recv_buffer[9])) / 100.0f);
					if (c->recv_buffer[7] == (uint8_t)1) z_pos *= -1.0f;

					//set the position of the closest camera:
					glm::vec3 received_pos = glm::vec3(x_pos, y_pos, z_pos);
					float curr_dist = 10000.0f;
					int cam_index = -1;
					for (int i = 0; i < cameras.size(); i++) {
						glm::vec3 cam_pos = (&cameras[i])->transform->position;
						float new_dist = std::sqrt(std::pow(received_pos.x - cam_pos.x, 2) + std::pow(received_pos.y - cam_pos.y, 2) + std::pow(received_pos.z - cam_pos.z, 2));
						if (new_dist < curr_dist) {
							curr_dist = new_dist;
							cam_index = i;
						}
					}
					if (camera != &cameras[cam_index])
						(&cameras[cam_index])->transform->position = received_pos;

					//consume this part of the buffer:
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 10);

					//for debugging:
					for (int i = 0; i < cameras.size(); i++) {
						Scene::Camera *vector_cam = &cameras[i];
						//std::cout << "Camera " << i << " position: " << vector_cam->transform->position.x << ", " << vector_cam->transform->position.y << ", " << vector_cam->transform->position.z << std::endl;
					}
				}
			}
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//from the game 2 base code:
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (camera != nullptr) {
		//update camera aspect ratio for drawable:
		camera->aspect = float(drawable_size.x) / float(drawable_size.y);

		//set up light type and position for lit_color_texture_program:
		// TODO: consider using the Light(s) in the scene to do this
		glUseProgram(lit_color_texture_program->program);
		glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
		GL_ERRORS();
		glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
		GL_ERRORS();
		glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
		GL_ERRORS();
		glUseProgram(0);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

		if (time_since_connection >= 0.2f)
			scene.draw(*camera);
	}
	//end of code from the game 2 base code

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		draw_text(glm::vec2(-aspect + 0.1f,-0.9f), server_message, 0.09f);
	}
	GL_ERRORS();
}

//from my game 3 code, which took these functions from Wikipedia:
//this function came from https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
glm::quat PlayMode::euler_to_quaternion(glm::vec3 euler) {
	//convert degrees to radians from http://cplusplus.com/forum/beginner/144006/
	glm::vec3 radians;
	radians.x = (euler.x * (float)M_PI) / 180.0f;
	radians.y = (euler.y * (float)M_PI) / 180.0f;
	radians.z = (euler.z * (float)M_PI) / 180.0f;

	//abbreviations for the various angular functions
	double cy = cos(radians.z * 0.5);
	double sy = sin(radians.z * 0.5);
	double cp = cos(radians.y * 0.5);
	double sp = sin(radians.y * 0.5);
	double cr = cos(radians.x * 0.5);
	double sr = sin(radians.x * 0.5);

	//get the quaternion to return
	glm::quat quaternion;
	quaternion.w = float(cr * cp * cy + sr * sp * sy);
	quaternion.x = float(sr * cp * cy - cr * sp * sy);
	quaternion.y = float(cr * sp * cy + sr * cp * sy);
	quaternion.z = float(cr * cp * sy - sr * sp * cy);

	return quaternion;
}

//this function came from https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
glm::vec3 PlayMode::quaternion_to_euler(glm::quat quaternion) {
	glm::vec3 euler;

	// roll (x-axis rotation)
	double sinr_cosp = 2 * (quaternion.w * quaternion.x + quaternion.y * quaternion.z);
	double cosr_cosp = 1 - 2 * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
	euler.x = float(std::atan2(sinr_cosp, cosr_cosp));

	// pitch (y-axis rotation)
	double sinp = 2 * (quaternion.w * quaternion.y - quaternion.z * quaternion.x);
	if (std::abs(sinp) >= 1)
		euler.y = float(std::copysign(M_PI / 2, sinp)); // use 90 degrees if out of range
	else
		euler.y = float(std::asin(sinp));

	// yaw (z-axis rotation)
	double siny_cosp = 2 * (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
	double cosy_cosp = 1 - 2 * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
	euler.z = float(std::atan2(siny_cosp, cosy_cosp));

	//convert to degrees from radians from http://cplusplus.com/forum/beginner/144006/
	euler.x = (euler.x * 180.0f) / (float)M_PI;
	euler.y = (euler.y * 180.0f) / (float)M_PI;
	euler.z = (euler.z * 180.0f) / (float)M_PI;

	return euler;
}
//end of code from my game 3 / Wikipedia

uint8_t PlayMode::absolute_float_to_uint8_t(float f) {
	f = std::abs(f);
	int tens = (int)f % 10;
	int ones = (int)f - (tens * 10);
	return tens * 10 + ones;
}

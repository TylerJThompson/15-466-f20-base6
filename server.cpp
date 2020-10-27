
#include "Connection.hpp"

#include "hex_dump.hpp"

#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>

#include <glm/glm.hpp>

int num_connected = 0;
int it_player = 0;

uint8_t absolute_float_to_uint8_t(float f) {
	f = std::abs(f);
	int tens = (int)f % 10;
	int ones = (int)f - (tens * 10);
	return tens * 10 + ones;
}

int main(int argc, char **argv) {
#ifdef _WIN32
	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif

	//------------ argument parsing ------------

	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}

	//------------ initialization ------------

	Server server(argv[1]);


	//------------ main loop ------------
	constexpr float ServerTick = 1.0f / 60.0f; //60fps is almost certainly over-kill, but idk, I like it lol

	//server state:

	//per-client state:
	struct PlayerInfo {
		PlayerInfo() {
			static uint32_t next_player_id = 1;
			id = next_player_id;
			name = "Player" + std::to_string(next_player_id);
			next_player_id += 1;

			position = glm::vec3(0.0f, 0.0f, -10.0f);
			//euler_angles = glm::vec3(45.0f, 0.0f, 0.0f);
		}

		int id;
		std::string name;
		glm::vec3 position;
		glm::vec3 euler_angles;
	};
	std::unordered_map< Connection *, PlayerInfo > players;

	while (true) {
		static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration< double >(ServerTick);
		//process incoming data from clients until a tick has elapsed:
		while (true) {
			auto now = std::chrono::steady_clock::now();
			double remain = std::chrono::duration< double >(next_tick - now).count();
			if (remain < 0.0) {
				next_tick += std::chrono::duration< double >(ServerTick);
				break;
			}
			server.poll([&](Connection *c, Connection::Event evt){
				if (evt == Connection::OnOpen) {
					//client connected:
					num_connected++;

					//create some player info for them:
					players.emplace(c, PlayerInfo());

					//find out which player the client is:
					std::string connection_message = "Client is player ";
					connection_message.append(std::to_string(num_connected));
					std::cout << connection_message << std::endl;

					//let the client know which player it is:
					c->send('m');
					c->send(uint8_t(connection_message.size() >> 16));
					c->send(uint8_t((connection_message.size() >> 8) % 256));
					c->send(uint8_t(connection_message.size() % 256));
					c->send_buffer.insert(c->send_buffer.end(), connection_message.begin(), connection_message.end());
				} else if (evt == Connection::OnClose) {
					//client disconnected:
					//num_connected--;

					//remove them from the players list:
					auto f = players.find(c);
					assert(f != players.end());
					players.erase(f);


				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					//std::cout << "got bytes:\n" << hex_dump(c->recv_buffer); std::cout.flush();

					//look up in players list:
					auto f = players.find(c);
					assert(f != players.end());
					PlayerInfo &player = f->second;

					//handle messages from client:
					while (c->recv_buffer.size() >= 10) {
						//expecting 'f' + seven float messages 'f' (x_pos) (y_pos) (z_pos) (x_rot) (y_rot) (z_rot)
						char type = c->recv_buffer[0];
						if (type != 'b') {
							std::cout << " message of non-'b' type received from client!" << std::endl;
							//shut down client connection:
							c->close();
							return;
						}

						//for getting a float out of a string: https://www.programiz.com/cpp-programming/string-float-conversion
						//construct position floats
						float client_x = std::stof(std::to_string(c->recv_buffer[2]));
						client_x += (std::stof(std::to_string(c->recv_buffer[3])) / 100.0f);
						if (c->recv_buffer[1] == (uint8_t)1) client_x *= -1.0f;
						float client_y = std::stof(std::to_string(c->recv_buffer[5]));
						client_y += (std::stof(std::to_string(c->recv_buffer[6])) / 100.0f);
						if (c->recv_buffer[4] == (uint8_t)1) client_y *= -1.0f;
						float client_z = std::stof(std::to_string(c->recv_buffer[8]));
						client_z += (std::stof(std::to_string(c->recv_buffer[9])) / 100.0f);
						if (c->recv_buffer[7] == (uint8_t)1) client_z *= -1.0f;

						//set the position of this player:
						player.position = glm::vec3(client_x, client_y, client_z);

						//consume this part of the buffer:
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 10);
					}
				}
			}, remain);
		}

		//update current game state
		glm::vec3 positions[4] = { };
		for (auto &[c, player] : players) {
			(void)c; //work around "unused variable" warning on whatever version of g++ github actions is running
			positions[player.id] = player.position;
		}

		//send updated game state to all clients
		for (auto &[c, player_unused] : players) {
			(void)player_unused; //work around "unused variable" warning on whatever version of g++ github actions is running
			for (auto &[c_unused, player] : players) {
				(void)c_unused; //work around "unused variable" warning on whatever version of g++ github actions is running
				
				//send an update starting with 'f', then 24 floats representing the camera transforms:
				c->send('t');

				//get the sign for each axis of the player position:
				uint8_t x_pos_sign = 0;
				if (player.position.x < 0.0f) x_pos_sign = 1;
				uint8_t y_pos_sign = 0;
				if (player.position.y < 0.0f) y_pos_sign = 1;
				uint8_t z_pos_sign = 0;
				if (player.position.z < 0.0f) z_pos_sign = 1;

				//get the digits to the left of the deimcal point:
				uint8_t x_pos = absolute_float_to_uint8_t(player.position.x);
				uint8_t y_pos = absolute_float_to_uint8_t(player.position.y);
				uint8_t z_pos = absolute_float_to_uint8_t(player.position.z);

				//get the first two digits to the right of the decimal point:
				uint8_t x_pos_decimal = absolute_float_to_uint8_t((std::abs(player.position.x) - x_pos) * 100.0f);
				uint8_t y_pos_decimal = absolute_float_to_uint8_t((std::abs(player.position.y) - y_pos) * 100.0f);
				uint8_t z_pos_decimal = absolute_float_to_uint8_t((std::abs(player.position.z) - z_pos) * 100.0f);

				//queue the current player position:
				c->send(x_pos_sign);
				c->send(x_pos);
				c->send(x_pos_decimal);
				c->send(y_pos_sign);
				c->send(y_pos);
				c->send(y_pos_decimal);
				c->send(z_pos_sign);
				c->send(z_pos);
				c->send(z_pos_decimal);

				//TODO: queue the current player rotation in euler angles
			}

			//send info for who is it
		}

	}

	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}

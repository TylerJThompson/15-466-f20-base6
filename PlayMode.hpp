#include "Mode.hpp"

#include "Connection.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	//keeps track of which player this instance of the client is
	int client_num = -1;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//transforms for the roofs; used for building detection:
	std::list< Scene::Transform* > roof_transforms;

	//camera:
	std::vector<Scene::Camera> cameras;
	Scene::Camera *camera = nullptr;
	float time_since_connection = 0.0f;

	//code from my game 3:
	//functions for helping with the camera rotation since I understand Euler angles way better than quaternions:
	//these functions are from https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	glm::quat euler_to_quaternion(glm::vec3 euler);
	glm::vec3 quaternion_to_euler(glm::quat quaternion);
	//end of code from my game 3

	//for converting from the absolute value of a float to a uint8_t:
	uint8_t absolute_float_to_uint8_t(float f);

	//for use in the tag game:
	bool player_is_it = false;
	float time_it = 5000.0f;
};

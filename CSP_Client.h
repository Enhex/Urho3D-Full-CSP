#pragma once

#include "CSP_messages.h"
#include "StateSnapshot.h"
#include <Urho3D/Scene/Component.h>
#include <vector>

namespace Urho3D
{
	class Context;
	class Controls;
	class Connection;
	class MemoryBuffer;
}

using namespace Urho3D;


/*
Client side prediction client.

- sends input to server
- receive state snapshot from server and run prediction
*/
struct CSP_Client : Component
{
	URHO3D_OBJECT(CSP_Client, Component);

	CSP_Client(Context* context);

	using ID = unsigned;

	// Register object factory and attributes.
	static void RegisterObject(Context* context);

	// Current's tick controls input
	Controls const* current_controls = nullptr;

	// Tags the input with "id" extraData, adds it to the input buffer
	void add_input(Controls& input);

	// do client-side prediction
	void predict();

	bool enable_copy = true;//used for testing

protected:
	StateSnapshot snapshot;

	// current client-side update ID
	ID id = 0;
	// The current recieved ID from the server
	ID server_id = -1;

	// Input buffer
	std::vector<Controls> input_buffer;

	// reusable buffer
	VectorBuffer input_message;


	// Handle custom network messages
	void HandleNetworkMessage(StringHash eventType, VariantMap& eventData);


	// read server's last received ID
	void read_last_id(MemoryBuffer& message);

	// Sends the client's input to the server
	void send_input(Controls& controls);

	// Re-apply all the inputs since after the current server ID to the current ID to correct the current network state.
	void reapply_inputs();

	// Remove all the elements in the buffer which are behind the server_id, including it since it was already applied.
	void remove_obsolete_history();
};

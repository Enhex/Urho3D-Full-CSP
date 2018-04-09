#ifndef CLIENT_SIDE_REDICTION_H
#define CLIENT_SIDE_REDICTION_H

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


//TODO needed?
//URHO3D_EVENT(E_CSP_UPDATE, CSP_update)
//{
//	URHO3D_PARAM(P_TIMESTEP, TimeStep); // float
//}

// remote event: server's last received input ID
static const StringHash E_CSP_last_input("CSP_last_input");
static const StringHash P_CSP_ID("CSP_ID");

/*
Client side prediction.
*/
struct ClientSidePrediction : Component
{
	URHO3D_OBJECT(ClientSidePrediction, Component);

	ClientSidePrediction(Context* context);

	using ID = unsigned;

	// Register object factory and attributes.
	static void RegisterObject(Context* context);

	// scene that replicates in the background
	SharedPtr<Scene> replication_scene;

	// Current's tick controls input
	Controls* current_controls = nullptr;
	bool enable_copy = true;//TODO used for testing

	bool Connect(const String& address, unsigned short port, const VariantMap& identity = Variant::emptyVariantMap);

	// Tags the input with "id" extraData, adds it to the input buffer
	void add_input(Controls& input);

	// send the last received input's ID
	void send_input_ID(Connection* client);

	// do client-side prediction
	void predict();

protected:
	// current client-side update ID
	ID id = 0;
	// The current recieved ID from the server
	ID server_id = -1;

	// Input buffer
	std::vector<Controls> input_buffer;

	// Re-apply all the inputs since after the current server ID to the current ID to correct the current network state.
	void reapply_inputs();

	// Remove all the elements in the buffer which are behind the server_id, including it since it was already applied.
	void remove_obsolete_history();

	void copy_scene();

	void HandleLastInput(StringHash eventType, VariantMap& eventData);
	void HandleNetworkUpdate(StringHash eventType, VariantMap& eventData);
};

void copy_node(Node& source, Node& destination);

void copy_child_nodes(Node& source, Node& destination);

void copy_component(Component& source_component, Node& destination_node);

// copy network attributes
void copy_attributes(Serializable& source, Serializable& destination);


#endif//guard

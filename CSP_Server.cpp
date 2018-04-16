#include "CSP_Client.h"
#include "CSP_Server.h"

#include "copy_node.h"
#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/DebugHud.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>

CSP_Server::CSP_Server(Context * context) :
	Component(context)
{
	SubscribeToEvent(E_NETWORKMESSAGE, URHO3D_HANDLER(CSP_Server, HandleNetworkMessage));
	SubscribeToEvent(E_RENDERUPDATE, URHO3D_HANDLER(CSP_Server, HandleRenderUpdate));
}

void CSP_Server::RegisterObject(Context * context)
{
	context->RegisterFactory<CSP_Server>();
}


void CSP_Server::read_input(Connection * connection, MemoryBuffer & message)
{
	if (!connection->IsClient())
	{
		URHO3D_LOGWARNING("Received unexpected Controls message from server");
		return;
	}

	Controls newControls;
	newControls.buttons_ = message.ReadUInt();
	newControls.yaw_ = message.ReadFloat();
	newControls.pitch_ = message.ReadFloat();
	newControls.extraData_ = message.ReadVariantMap();

	auto& controls = client_inputs[connection];

	// make sure the new input is more recent
	//TODO handle looping
	if (newControls.extraData_["id"].GetUInt() > controls.extraData_["id"].GetUInt()) {
		controls = newControls;
		client_last_IDs[connection] = controls.extraData_["id"].GetUInt();
	}


	//TODO replace with an array of client inputs that the server code can use, instead of passing it via function parameter?
	//apply_client_input(newControls, timestep, connection);
}


void add_replicated_node(StateSnapshot& snapshot, Node* node);

void add_replicated_child_nodes(StateSnapshot& snapshot, Node* node) {
	const auto& children = node->GetChildren();
	for (auto child : children)
		add_replicated_node(snapshot, child);
};

void add_replicated_node(StateSnapshot& snapshot, Node* node) {
	if (!node->IsReplicated())
		return;

	snapshot.add_node(node);

	add_replicated_child_nodes(snapshot, node);
};


void CSP_Server::prepare_state_snapshot()
{
	state.Clear();

	// Write placeholder last input ID, which will be set per connection before sending
	state.WriteUInt(0);

	auto scene = GetScene();
	//scene->ApplyAttributes();

	//snapshot.nodes.clear();
	//add_replicated_child_nodes(snapshot, scene);

	// write state snapshot
	snapshot.write_state(state, GetScene());
}

void CSP_Server::send_state_updates()
{
	auto network = GetSubsystem<Network>();
	auto client_connections = network->GetClientConnections();

	for (auto connection : client_connections)
		send_state_update(connection);
}

void CSP_Server::send_state_update(Connection * connection)
{
	// Set the last input ID per connection
	//unsigned int last_id = client_inputs[connection].extraData_["id"].GetUInt();
	unsigned int last_id = client_last_IDs[connection];
	
	GetSubsystem<DebugHud>()->SetAppStats("last_id: ", last_id);

	state.Seek(0);
	state.WriteUInt(last_id);

	connection->SendMessage(MSG_CSP_STATE, false, false, state);
}

void CSP_Server::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
	auto network = GetSubsystem<Network>();

	auto timeStep = eventData[RenderUpdate::P_TIMESTEP].GetFloat();

	// Check if periodic update should happen now
	updateAcc_ += timeStep;
	bool updateNow = updateAcc_ >= updateInterval_;

	if (updateNow && network->IsServerRunning())
	{
		updateAcc_ = fmodf(updateAcc_, updateInterval_);

		prepare_state_snapshot();
		send_state_updates();
	}
}

void CSP_Server::HandleNetworkMessage(StringHash eventType, VariantMap & eventData)
{
	auto network = GetSubsystem<Network>();

	using namespace NetworkMessage;
	const auto message_id = eventData[P_MESSAGEID].GetInt();
	auto connection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());
	MemoryBuffer message(eventData[P_DATA].GetBuffer());

	if (network->IsServerRunning())
	{
		switch (message_id)
		{
		case MSG_CSP_INPUT:
			read_input(connection, message);
			break;
		}
	}
}

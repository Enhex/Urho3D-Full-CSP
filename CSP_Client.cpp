#include "CSP_Client.h"

#include "copy_node.h"
#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/DebugHud.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/Physics/PhysicsEvents.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/Scene/SmoothedTransform.h>


CSP_Client::CSP_Client(Context * context) :
	Component(context)
{
	SubscribeToEvent(E_NETWORKMESSAGE, URHO3D_HANDLER(CSP_Client, HandleNetworkMessage));
}

void CSP_Client::RegisterObject(Context * context)
{
	context->RegisterFactory<CSP_Client>();
}


void CSP_Client::add_input(Controls& new_input)
{
	// Increment the update ID by 1
	++id;
	// Tag the new input with an id, so the id is passed to the server
	new_input.extraData_["id"] = id;
	// Add the new input to the input buffer
	input_buffer.push_back(new_input);

	// Send to the server
	send_input(new_input);

	GetSubsystem<DebugHud>()->SetAppStats("client id: ", id);
}

void CSP_Client::send_input(Controls & controls)
{
	auto server_connection = GetSubsystem<Network>()->GetServerConnection();
	if (!server_connection ||
		!server_connection->GetScene() ||
		!server_connection->IsSceneLoaded())
		return;

	input_message.Clear();
	input_message.WriteUInt(controls.buttons_);
	input_message.WriteFloat(controls.yaw_);
	input_message.WriteFloat(controls.pitch_);
	input_message.WriteVariantMap(controls.extraData_);

	server_connection->SendMessage(MSG_CSP_INPUT, false, false, input_message);
}

void CSP_Client::read_last_id(MemoryBuffer & message)
{
	// Read last input ID
	auto new_server_id = message.ReadUInt();

	// Make sure it's more recent than the previous last ID since we're sending unordered messages
	// Handle range looping correctly
	if (id > server_id) {
		if (new_server_id < server_id)
			return;
	}
	else {
		if (new_server_id > server_id)
			return;
	}

	server_id = new_server_id;
	GetSubsystem<DebugHud>()->SetAppStats("server_id: ", server_id);
}


void CSP_Client::predict()
{
	//copy_scene();

	remove_obsolete_history();
	reapply_inputs();
}


void CSP_Client::reapply_inputs()
{
	auto scene = GetScene();
	auto physicsWorld = scene->GetComponent<PhysicsWorld>();
	const auto timestep = 1.f / physicsWorld->GetFps();

	// step a tick
	for (auto& controls : input_buffer)
	{
		current_controls = &controls;
		physicsWorld->Update(timestep);
	}

	// current controls should only be used while predicting
	current_controls = nullptr;
}


void CSP_Client::remove_obsolete_history()
{
	std::vector<Controls> new_input_buffer;

	for (size_t i = 0; i < input_buffer.size(); ++i)
	{
		auto& controls = input_buffer[i];
		unsigned update_id = controls.extraData_["id"].GetUInt();
		bool remove = false;

		// Handle value range looping correctly
		if (id > server_id)
		{
			if (update_id <= server_id ||
				update_id > id)
				remove = true;
		}
		else
		{
			if (update_id >= server_id ||
				update_id < id)
				remove = true;
		}

		if (!remove)
			new_input_buffer.push_back(controls);
	}

	input_buffer = std::move(new_input_buffer);
}


void CSP_Client::HandleNetworkMessage(StringHash eventType, VariantMap & eventData)
{
	auto network = GetSubsystem<Network>();

	using namespace NetworkMessage;
	const auto message_id = eventData[P_MESSAGEID].GetInt();
	auto connection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());
	MemoryBuffer message(eventData[P_DATA].GetBuffer());

	if (network->GetServerConnection())
	{
		switch (message_id)
		{
		case MSG_CSP_STATE:
			// read last input
			read_last_id(message);

			if (enable_copy) {
				// read state snapshot
				auto scene = network->GetServerConnection()->GetScene();
				snapshot.read_state(message, scene);
				scene->ApplyAttributes();
			}

			// Perform client side prediction
			predict();

			break;
		}
	}
}

#include "ClientSidePrediction.h"

#include "copy_node.h"
#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/Physics/PhysicsEvents.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/Scene/SmoothedTransform.h>


ClientSidePrediction::ClientSidePrediction(Context * context) :
	Component(context)
{
	replication_scene = MakeShared<Scene>(context);
	auto physicsWorld = replication_scene->CreateComponent<PhysicsWorld>(LOCAL);
	physicsWorld->SetUpdateEnabled(false);//TODO null at this point

	//SubscribeToEvent(E_CSP_last_input, URHO3D_HANDLER(ClientSidePrediction, HandleLastInput));
	//SubscribeToEvent(E_NETWORKUPDATE, URHO3D_HANDLER(ClientSidePrediction, HandleNetworkUpdate));

	GetSubsystem<Network>()->RegisterRemoteEvent(E_CSP_last_input);
}


void ClientSidePrediction::RegisterObject(Context* context)
{
	context->RegisterFactory<ClientSidePrediction>();
}

bool ClientSidePrediction::Connect(const String & address, unsigned short port, const VariantMap & identity)
{
	auto network = GetSubsystem<Network>();
	return network->Connect(address, port, replication_scene);

	// CSP is going to manually update the client's scene
	GetScene()->SetUpdateEnabled(false);
	replication_scene->SetUpdateEnabled(false);
}



void ClientSidePrediction::send_input_ID(Connection* client)
{
	VariantMap remoteEventData;
	auto id_data = client->GetControls().extraData_["id"];
	if (id_data != nullptr) {
		remoteEventData[P_CSP_ID] = id_data->GetUInt();
		client->SendRemoteEvent(E_CSP_last_input, false, remoteEventData);
	}
}


void ClientSidePrediction::test_predict(const Controls& input)
{
	copy_scene();

	auto network = GetSubsystem<Network>();
	const auto timestep = 1.f / network->GetUpdateFps();

	auto scene = GetScene();

	for (unsigned i = 20; i-- > 0;)
	{
		// step a tick
		current_controls = &input;
		scene->Update(timestep);
	}
}


void ClientSidePrediction::copy_scene()
{
	auto scene = GetScene();
	if(scene)
		copy_child_nodes(*replication_scene, *scene);
}

void ClientSidePrediction::HandleLastInput(StringHash eventType, VariantMap & eventData)
{
	//server_id = eventData[P_CSP_ID].GetUInt();
}

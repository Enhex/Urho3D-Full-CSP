#pragma once

#include <Urho3D/Scene/Component.h>
#include <vector>
#include "StateSnapshot.h"

namespace Urho3D
{
	class Context;
	class Controls;
	class Connection;
	class MemoryBuffer;
}

using namespace Urho3D;


// remote event: server's last received input ID
static const StringHash E_CSP_last_input("CSP_last_input");
static const StringHash P_CSP_ID("CSP_ID");


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
	Controls const* current_controls = nullptr;
	bool enable_copy = true;//TODO used for testing

	bool Connect(const String& address, unsigned short port, const VariantMap& identity = Variant::emptyVariantMap);

	// send the last received input's ID
	void send_input_ID(Connection* client);




	void test_predict(const Controls& input);

protected:
	// Client input ID map
	HashMap<Connection*, ID> client_input_IDs;


	void copy_scene();

	void HandleLastInput(StringHash eventType, VariantMap& eventData);
};

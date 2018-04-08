#include "ClientSidePrediction.h"

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/IO/Log.h>
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

	SubscribeToEvent(E_CSP_last_input, URHO3D_HANDLER(ClientSidePrediction, HandleLastInput));

	SubscribeToEvent(E_SCENEUPDATE, [&](StringHash type, VariantMap& args) {
		copy_scene();
	});
}


void ClientSidePrediction::RegisterObject(Context* context)
{
	context->RegisterFactory<ClientSidePrediction>();
}

bool ClientSidePrediction::Connect(const String & address, unsigned short port, const VariantMap & identity)
{
	auto network = GetSubsystem<Network>();
	return network->Connect(address, port, replication_scene);
}


void ClientSidePrediction::add_input(Controls& new_input)
{
	// Increment the update ID by 1
	++id;
	// Tag the new input with an id, so the id is passed to the server
	new_input.extraData_["id"] = id;
	// Add the new input to the input buffer
	input_buffer.push_back(new_input);
}


//
// predict
//
void ClientSidePrediction::predict()
{
	remove_obsolete_history();
	reapply_inputs();
}


//
// reapply_inputs
//
void ClientSidePrediction::reapply_inputs()
{
	auto network = GetSubsystem<Network>();
	const auto timestep = 1.f / network->GetUpdateFps();

	for (auto& controls : input_buffer)
	{
		// step a tick
		//TODO
	}
}


//
// remove_obsolete_history
//
void ClientSidePrediction::remove_obsolete_history()
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

void ClientSidePrediction::copy_scene()
{
	auto scene = GetScene();
	copy_child_nodes(*replication_scene, *scene);
}

void ClientSidePrediction::HandleLastInput(StringHash eventType, VariantMap & eventData)
{
}

void copy_node(Node & source, Node & destination)
{
	// copy attributes
	copy_attributes(source, destination);
	// ApplyAttributes() is deliberately skipped, as Node has no attributes that require late applying.
	// Furthermore it would propagate to components and child nodes, which is not desired in this case

	// copy user variables
	for (const auto& var : source.GetVars()) {
		destination.SetVar(var.first_, var.second_);
	}

	// copy components
	for (const auto component : source.GetComponents())
	{
		if (!component->IsReplicated())
			continue;

		auto dest_component = destination.GetScene()->GetComponent(component->GetID());
		copy_component(*component, destination);
	}

	// copy child nodes
	copy_child_nodes(source, destination);

	//destination.ApplyAttributes();
}

void copy_child_nodes(Node & source, Node & destination)
{
	auto dest_scene = destination.GetScene();
	for (auto source_node : source.GetChildren())
	{
		if (!source_node->IsReplicated())
			continue;

		auto destination_node = dest_scene->GetNode(source_node->GetID());

		// Create the node if it doesn't exist
		if (!destination_node)
		{
			// Add initially to the root level. May be moved as we receive the parent attribute
			destination_node = destination.CreateChild(source_node->GetID(), REPLICATED);
			// Create smoothed transform component
			destination_node->CreateComponent<SmoothedTransform>(LOCAL);
		}

		copy_node(*source_node, *destination_node);
	}
}

void copy_component(Component & source_component, Node& destination_node)
{
	// Check if the component by this ID and type already exists in this node
	auto dest_component = destination_node.GetScene()->GetComponent(source_component.GetID());
	if (!dest_component || dest_component->GetType() != source_component.GetType() || dest_component->GetNode() != &destination_node)
	{
		if (dest_component)
			dest_component->Remove();
		dest_component = destination_node.CreateComponent(source_component.GetType(), REPLICATED, source_component.GetID());
	}

	// If was unable to create the component, would desync the message and therefore have to abort
	if (!dest_component)
	{
		URHO3D_LOGERROR("CreateNode message parsing aborted due to unknown component");
		return;
	}

	// Read attributes and apply
	copy_attributes(source_component, *dest_component);
	dest_component->ApplyAttributes();
}

void copy_attributes(Serializable & source, Serializable & destination)
{
	const auto attributes = source.GetNetworkAttributes();
	if (!attributes)
		return;

	Variant value;

	const auto numAttributes = attributes->Size();
	for (unsigned i = 0; i < numAttributes; ++i)
	{
		const auto& attr = attributes->At(i);
		value.Clear();
		source.OnGetAttribute(attr, value);
		destination.OnSetAttribute(attr, value);
	}
}

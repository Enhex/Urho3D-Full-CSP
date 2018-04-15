#include "copy_node.h"

#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/Component.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SmoothedTransform.h>

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
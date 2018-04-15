#pragma once

namespace Urho3D
{
	class Node;
	class Component;
	class Serializable;
}

using namespace Urho3D;


void copy_node(Node& source, Node& destination);

void copy_child_nodes(Node& source, Node& destination);

void copy_component(Component& source_component, Node& destination_node);

// copy network attributes
void copy_attributes(Serializable& source, Serializable& destination);
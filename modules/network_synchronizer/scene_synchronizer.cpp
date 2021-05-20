/*************************************************************************/
/*  scene_synchronizer.cpp                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

/**
	@author AndreaCatania
*/

#include "scene_synchronizer.h"

#include "net_utilities.h"
#include "networked_controller.h"
#include "scene/main/window.h"

SceneSynchronizer::VarData::VarData() {}

SceneSynchronizer::VarData::VarData(StringName p_name) {
	var.name = p_name;
}

SceneSynchronizer::VarData::VarData(uint32_t p_id, StringName p_name, Variant p_val, bool p_skip_rewinding, bool p_enabled) :
		id(p_id),
		skip_rewinding(p_skip_rewinding),
		enabled(p_enabled) {
	var.name = p_name;
	var.value = p_val.duplicate(true);
}

bool SceneSynchronizer::VarData::operator==(const VarData &p_other) const {
	return var.name == p_other.var.name;
}

SceneSynchronizer::NodeData::NodeData() {}

int64_t SceneSynchronizer::NodeData::find_var_by_id(uint32_t p_id) const {
	if (p_id == 0) {
		return -1;
	}
	const VarData *ptr = vars.ptr();
	for (int i = 0; i < vars.size(); i += 1) {
		if (ptr[i].id == p_id) {
			return i;
		}
	}
	return -1;
}

void SceneSynchronizer::NodeData::process(const real_t p_delta) const {
	const Variant var_delta = p_delta;
	const Variant *fake_array_vars = &var_delta;

	Callable::CallError e;
	for (uint32_t i = 0; i < functions.size(); i += 1) {
		node->call(functions[i], &fake_array_vars, 1, e);
	}
}

SceneSynchronizer::Snapshot::operator String() const {
	String s;
	s += "Snapshot input ID: " + itos(input_id);

	for (
			OAHashMap<ObjectID, Vector<SceneSynchronizer::VarData>>::Iterator it = node_vars.iter();
			it.valid;
			it = node_vars.next_iter(it)) {
		s += "\nNode Data: ";
		if (nullptr != ObjectDB::get_instance(*it.key))
			s += static_cast<Node *>(ObjectDB::get_instance(*it.key))->get_path();
		else
			s += " (Object ID): " + itos(*it.key);
		for (int i = 0; i < it.value->size(); i += 1) {
			s += "\n|- Variable: ";
			s += (*it.value)[i].var.name;
			s += " = ";
			s += String((*it.value)[i].var.value);
		}
	}
	return s;
}

void SceneSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reset_synchronizer_mode"), &SceneSynchronizer::reset_synchronizer_mode);
	ClassDB::bind_method(D_METHOD("clear"), &SceneSynchronizer::clear);

	ClassDB::bind_method(D_METHOD("set_server_notify_state_interval", "interval"), &SceneSynchronizer::set_server_notify_state_interval);
	ClassDB::bind_method(D_METHOD("get_server_notify_state_interval"), &SceneSynchronizer::get_server_notify_state_interval);

	ClassDB::bind_method(D_METHOD("set_comparison_float_tolerance", "tolerance"), &SceneSynchronizer::set_comparison_float_tolerance);
	ClassDB::bind_method(D_METHOD("get_comparison_float_tolerance"), &SceneSynchronizer::get_comparison_float_tolerance);

	ClassDB::bind_method(D_METHOD("register_variable", "node", "variable", "on_change_notify", "skip_rewinding"), &SceneSynchronizer::register_variable, DEFVAL(StringName()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("unregister_variable", "node", "variable"), &SceneSynchronizer::unregister_variable);

	ClassDB::bind_method(D_METHOD("get_changed_event_name", "variable"), &SceneSynchronizer::get_changed_event_name);

	ClassDB::bind_method(D_METHOD("track_variable_changes", "node", "variable", "method"), &SceneSynchronizer::track_variable_changes);
	ClassDB::bind_method(D_METHOD("untrack_variable_changes", "node", "variable", "method"), &SceneSynchronizer::untrack_variable_changes);

	ClassDB::bind_method(D_METHOD("set_node_as_controlled_by", "node", "controller"), &SceneSynchronizer::set_node_as_controlled_by);

	ClassDB::bind_method(D_METHOD("register_process", "node", "function"), &SceneSynchronizer::register_process);
	ClassDB::bind_method(D_METHOD("unregister_process", "node", "function"), &SceneSynchronizer::unregister_process);

	ClassDB::bind_method(D_METHOD("is_recovered"), &SceneSynchronizer::is_recovered);
	ClassDB::bind_method(D_METHOD("is_resetted"), &SceneSynchronizer::is_resetted);
	ClassDB::bind_method(D_METHOD("is_rewinding"), &SceneSynchronizer::is_rewinding);

	ClassDB::bind_method(D_METHOD("force_state_notify"), &SceneSynchronizer::force_state_notify);

	ClassDB::bind_method(D_METHOD("_on_peer_connected"), &SceneSynchronizer::_on_peer_connected);
	ClassDB::bind_method(D_METHOD("_on_peer_disconnected"), &SceneSynchronizer::_on_peer_disconnected);

	ClassDB::bind_method(D_METHOD("__clear"), &SceneSynchronizer::__clear);
	ClassDB::bind_method(D_METHOD("_rpc_send_state"), &SceneSynchronizer::_rpc_send_state);
	ClassDB::bind_method(D_METHOD("_rpc_notify_need_full_snapshot"), &SceneSynchronizer::_rpc_notify_need_full_snapshot);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "server_notify_state_interval", PROPERTY_HINT_RANGE, "0.001,10.0,0.0001"), "set_server_notify_state_interval", "get_server_notify_state_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "comparison_float_tolerance", PROPERTY_HINT_RANGE, "0.000001,0.01,0.000001"), "set_comparison_float_tolerance", "get_comparison_float_tolerance");
}

void SceneSynchronizer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (Engine::get_singleton()->is_editor_hint())
				return;

			// TODO add a signal that allows to not check this each frame.
			if (unlikely(peer_ptr != get_multiplayer()->get_network_peer().ptr())) {
				reset_synchronizer_mode();
			}

			const int lowest_priority_number = INT32_MAX;
			ERR_FAIL_COND_MSG(get_process_priority() != lowest_priority_number, "The process priority MUST not be changed, it's likely there is a better way of doing what you are trying to do, if you really need it please open an issue.");

			process();
		} break;
		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint())
				return;

			__clear();
			reset_synchronizer_mode();

			get_multiplayer()->connect("network_peer_connected", Callable(this, "_on_peer_connected"));
			get_multiplayer()->connect("network_peer_disconnected", Callable(this, "_on_peer_disconnected"));

		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint())
				return;

			get_multiplayer()->disconnect("network_peer_connected", Callable(this, "_on_peer_connected"));
			get_multiplayer()->disconnect("network_peer_disconnected", Callable(this, "_on_peer_disconnected"));

			__clear();

			if (synchronizer) {
				memdelete(synchronizer);
				synchronizer = nullptr;
				synchronizer_type = SYNCHRONIZER_TYPE_NULL;
			}

			set_physics_process_internal(false);
		}
	}
}

SceneSynchronizer::SceneSynchronizer() {
	rpc_config("__clear", MultiplayerAPI::RPC_MODE_REMOTE);
	rpc_config("_rpc_send_state", MultiplayerAPI::RPC_MODE_REMOTE);
	rpc_config("_rpc_notify_need_full_snapshot", MultiplayerAPI::RPC_MODE_REMOTE);
}

SceneSynchronizer::~SceneSynchronizer() {
	__clear();
	if (synchronizer) {
		memdelete(synchronizer);
		synchronizer = nullptr;
		synchronizer_type = SYNCHRONIZER_TYPE_NULL;
	}
}

void SceneSynchronizer::set_server_notify_state_interval(real_t p_interval) {
	server_notify_state_interval = p_interval;
}

real_t SceneSynchronizer::get_server_notify_state_interval() const {
	return server_notify_state_interval;
}

void SceneSynchronizer::set_comparison_float_tolerance(real_t p_tolerance) {
	comparison_float_tolerance = p_tolerance;
}

real_t SceneSynchronizer::get_comparison_float_tolerance() const {
	return comparison_float_tolerance;
}

void SceneSynchronizer::register_variable(Node *p_node, StringName p_variable, StringName p_on_change_notify, bool p_skip_rewinding) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_variable == StringName());

	NodeData *node_data = register_node(p_node);
	ERR_FAIL_COND(node_data == nullptr);

	const int id = node_data->vars.find(p_variable);
	if (id == -1) {
		const Variant old_val = p_node->get(p_variable);
		const int var_id = generate_id ? node_data->vars.size() + 1 : 0;
		node_data->vars.push_back(
				VarData(
						var_id,
						p_variable,
						old_val,
						p_skip_rewinding,
						true));
	} else {
		VarData *ptr = node_data->vars.ptrw();
		ptr[id].skip_rewinding = p_skip_rewinding;
		ptr[id].enabled = true;
	}

	if (p_node->has_signal(get_changed_event_name(p_variable)) == false) {
		p_node->add_user_signal(MethodInfo(
				get_changed_event_name(p_variable)));
	}

	if (p_on_change_notify != StringName()) {
		track_variable_changes(p_node, p_variable, p_on_change_notify);
	}

	synchronizer->on_variable_added(node_data, p_variable);
}

void SceneSynchronizer::unregister_variable(Node *p_node, StringName p_variable) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_variable == StringName());

	NodeData *nd = get_node_data(p_node->get_instance_id());
	ERR_FAIL_COND(nd == nullptr);

	const int64_t index = nd->vars.find(p_variable);
	ERR_FAIL_COND(index == -1);

	// Disconnects the eventual connected methods
	List<Connection> connections;
	p_node->get_signal_connection_list(get_changed_event_name(p_variable), &connections);

	for (List<Connection>::Element *e = connections.front(); e != nullptr; e = e->next()) {
		p_node->disconnect(get_changed_event_name(p_variable), e->get().callable);
	}

	nd->vars.write[index].enabled = false;
}

String SceneSynchronizer::get_changed_event_name(StringName p_variable) {
	return "variable_" + p_variable + "_changed";
}

void SceneSynchronizer::track_variable_changes(Node *p_node, StringName p_variable, StringName p_method) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_variable == StringName());
	ERR_FAIL_COND(p_method == StringName());

	NodeData *nd = get_node_data(p_node->get_instance_id());
	ERR_FAIL_COND_MSG(nd == nullptr, "You need to register the variable to track its changes.");
	ERR_FAIL_COND_MSG(nd->vars.find(p_variable) == -1, "You need to register the variable to track its changes.");

	if (p_node->is_connected(
				get_changed_event_name(p_variable),
				Callable(p_node, p_method)) == false) {
		p_node->connect(
				get_changed_event_name(p_variable),
				Callable(p_node, p_method));
	}
}

void SceneSynchronizer::untrack_variable_changes(Node *p_node, StringName p_variable, StringName p_method) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_variable == StringName());
	ERR_FAIL_COND(p_method == StringName());

	NodeData *nd = get_node_data(p_node->get_instance_id());
	ERR_FAIL_COND(nd == nullptr);
	ERR_FAIL_COND(nd->vars.find(p_variable) == -1);

	if (p_node->is_connected(
				get_changed_event_name(p_variable),
				Callable(p_node, p_method))) {
		p_node->disconnect(
				get_changed_event_name(p_variable),
				Callable(p_node, p_method));
	}
}

void SceneSynchronizer::set_node_as_controlled_by(Node *p_node, Node *p_controller) {
	NodeData *nd = register_node(p_node);
	ERR_FAIL_COND(nd == nullptr);
	ERR_FAIL_COND_MSG(nd->is_controller, "A controller can't be controlled by another controller.");

	if (nd->controlled_by) {
#ifdef DEBUG_ENABLED
		CRASH_COND_MSG(global_nodes_node_data.find(nd) != -1, "There is a bug the same node is added twice into the global_nodes_node_data.");
#endif
		// Put the node back into global.
		global_nodes_node_data.push_back(nd);
		nd->controlled_by->controlled_nodes.erase(nd);
		nd->controlled_by = nullptr;
	}

	if (p_controller) {
		NetworkedController *c = Object::cast_to<NetworkedController>(p_controller);
		ERR_FAIL_COND_MSG(c == nullptr, "The controller must be a node of type: NetworkedController.");

		NodeData *controller_node_data = register_node(p_controller);
		ERR_FAIL_COND(controller_node_data == nullptr);
		ERR_FAIL_COND_MSG(controller_node_data->is_controller == false, "The node can be only controlled by a controller.");

#ifdef DEBUG_ENABLED
		CRASH_COND_MSG(controller_node_data->controlled_nodes.find(nd) != -1, "There is a bug the same node is added twice into the controlled_nodes.");
#endif
		controller_node_data->controlled_nodes.push_back(nd);
		global_nodes_node_data.erase(nd);
		nd->controlled_by = controller_node_data;
	}

#ifdef DEBUG_ENABLED
	// The controller is always registered before a node is marked to be
	// controlled by.
	// So assert that no controlled nodes are into globals.
	for (uint32_t i = 0; i < global_nodes_node_data.size(); i += 1) {
		CRASH_COND(global_nodes_node_data[i]->controlled_by != nullptr);
	}

	// And now make sure that all controlled nodes are into the proper controller.
	for (uint32_t i = 0; i < controllers_node_data.size(); i += 1) {
		for (uint32_t y = 0; y < controllers_node_data[i]->controlled_nodes.size(); y += 1) {
			CRASH_COND(controllers_node_data[i]->controlled_nodes[y]->controlled_by != controllers_node_data[i]);
		}
	}
#endif
}

void SceneSynchronizer::register_process(Node *p_node, StringName p_function) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_function == StringName());
	NodeData *node_data = register_node(p_node);
	ERR_FAIL_COND(node_data == nullptr);

	if (node_data->functions.find(p_function) == -1) {
		node_data->functions.push_back(p_function);
	}
}

void SceneSynchronizer::unregister_process(Node *p_node, StringName p_function) {
	ERR_FAIL_COND(p_node == nullptr);
	ERR_FAIL_COND(p_function == StringName());
	NodeData *node_data = register_node(p_node);
	ERR_FAIL_COND(node_data == nullptr);
	node_data->functions.erase(p_function);
}

bool SceneSynchronizer::is_recovered() const {
	return recover_in_progress;
}

bool SceneSynchronizer::is_resetted() const {
	return reset_in_progress;
}

bool SceneSynchronizer::is_rewinding() const {
	return rewinding_in_progress;
}

void SceneSynchronizer::force_state_notify() {
	ERR_FAIL_COND(synchronizer_type != SYNCHRONIZER_TYPE_SERVER);
	ServerSynchronizer *r = static_cast<ServerSynchronizer *>(synchronizer);
	// + 1.0 is just a ridiculous high number to be sure to avoid float
	// precision error.
	r->state_notifier_timer = get_server_notify_state_interval() + 1.0;
}

void SceneSynchronizer::_on_peer_connected(int p_peer) {
	peer_data.set(p_peer, PeerData());
}

void SceneSynchronizer::_on_peer_disconnected(int p_peer) {
	peer_data.remove(p_peer);
}

void SceneSynchronizer::reset_synchronizer_mode() {
	set_physics_process_internal(false);
	generate_id = false;

	if (synchronizer) {
		memdelete(synchronizer);
		synchronizer = nullptr;
		synchronizer_type = SYNCHRONIZER_TYPE_NULL;
	}

	peer_ptr = get_multiplayer()->get_network_peer().ptr();

	if (get_tree() == nullptr || get_tree()->get_network_peer().is_null()) {
		synchronizer_type = SYNCHRONIZER_TYPE_NONETWORK;
		synchronizer = memnew(NoNetSynchronizer(this));
		generate_id = true;

	} else if (get_tree()->is_network_server()) {
		synchronizer_type = SYNCHRONIZER_TYPE_SERVER;
		synchronizer = memnew(ServerSynchronizer(this));
		generate_id = true;
	} else {
		synchronizer_type = SYNCHRONIZER_TYPE_CLIENT;
		synchronizer = memnew(ClientSynchronizer(this));
	}

	// Always runs the SceneSynchronizer last.
	const int lowest_priority_number = INT32_MAX;
	set_process_priority(lowest_priority_number);
	set_physics_process_internal(true);

	if (synchronizer) {
		// Notify the presence all available nodes and its variables to the synchronizer.
		for (uint32_t i = 0; i < node_data.size(); i += 1) {
			synchronizer->on_node_added(node_data[i]);
			for (int y = 0; y < node_data[i]->vars.size(); y += 1) {
				synchronizer->on_variable_added(node_data[i], node_data[i]->vars[y].var.name);
			}
		}
	}
}

void SceneSynchronizer::clear() {
	if (synchronizer_type == SYNCHRONIZER_TYPE_NONETWORK) {
		__clear();
	} else {
		ERR_FAIL_COND_MSG(get_tree()->is_network_server() == false, "The clear function must be called on server");
		__clear();
		rpc("__clear");
	}
}

void SceneSynchronizer::__clear() {
	for (uint32_t i = 0; i < node_data.size(); i += 1) {
		Node *node = static_cast<Node *>(ObjectDB::get_instance(node_data[i]->instance_id));
		if (node != nullptr) {
			for (int y = 0; y < node_data[i]->vars.size(); y += 1) {
				// Unregister the variable so the connected variables are
				// correctly removed
				unregister_variable(node, node_data[i]->vars[y].var.name);
			}
		}
	}

	node_data.clear();
	controllers_node_data.clear();
	global_nodes_node_data.clear();
	node_counter = 1;

	if (synchronizer) {
		synchronizer->clear();
	}
}

void SceneSynchronizer::_rpc_send_state(Variant p_snapshot) {
	ERR_FAIL_COND(get_tree()->is_network_server() == true);
	synchronizer->receive_snapshot(p_snapshot);
}

void SceneSynchronizer::_rpc_notify_need_full_snapshot() {
	ERR_FAIL_COND(get_tree()->is_network_server() == false);

	const int sender_peer = get_tree()->get_multiplayer()->get_rpc_sender_id();
	PeerData *pd = peer_data.lookup_ptr(sender_peer);
	ERR_FAIL_COND(pd == nullptr);
	pd->need_full_snapshot = true;
}

void SceneSynchronizer::update_peers() {
	if (peer_dirty == false) {
		return;
	}
	peer_dirty = false;

	for (uint32_t i = 0; i < controllers_node_data.size(); i += 1) {
		PeerData *pd = peer_data.lookup_ptr(controllers_node_data[i]->node->get_network_master());
		if (pd) {
			pd->controller_id = controllers_node_data[i]->instance_id;
		}
	}
}

SceneSynchronizer::NodeData *SceneSynchronizer::register_node(Node *p_node) {
	ERR_FAIL_COND_V(p_node == nullptr, nullptr);

	NodeData *nd = get_node_data(p_node->get_instance_id());
	if (unlikely(nd == nullptr)) {
		const uint32_t node_id(generate_id ? node_counter++ : 0);
		nd = memnew(NodeData);
		nd->id = node_id;
		nd->instance_id = p_node->get_instance_id();
		nd->node = p_node;
		node_data.push_back(nd);

		NetworkedController *controller = Object::cast_to<NetworkedController>(p_node);
		if (controller) {
			if (unlikely(controller->has_scene_synchronizer())) {
				node_data.erase(nd);
				memdelete(nd);
				ERR_FAIL_V_MSG(nullptr, "This controller already has a synchronizer. This is a bug!");
			}

			nd->is_controller = true;
			controllers_node_data.push_back(nd);

			controller->set_scene_synchronizer(this);
			peer_dirty = true;

		} else {
			nd->is_controller = false;
			global_nodes_node_data.push_back(nd);
		}

		synchronizer->on_node_added(nd);

		NET_DEBUG_PRINT("New node registered, ID: " + itos(node_id) + ". Node: " + p_node->get_path());
	}
	return nd;
}

bool SceneSynchronizer::vec2_evaluation(const Vector2 a, const Vector2 b) {
	return (a - b).length_squared() <= (comparison_float_tolerance * comparison_float_tolerance);
}

bool SceneSynchronizer::vec3_evaluation(const Vector3 a, const Vector3 b) {
	return (a - b).length_squared() <= (comparison_float_tolerance * comparison_float_tolerance);
}

bool SceneSynchronizer::synchronizer_variant_evaluation(const Variant &v_1, const Variant &v_2) {
	if (v_1.get_type() != v_2.get_type()) {
		return false;
	}

	// Custom evaluation methods
	switch (v_1.get_type()) {
		case Variant::FLOAT: {
			const real_t a(v_1);
			const real_t b(v_2);
			return ABS(a - b) <= comparison_float_tolerance;
		}
		case Variant::VECTOR2: {
			return vec2_evaluation(v_1, v_2);
		}
		case Variant::RECT2: {
			const Rect2 a(v_1);
			const Rect2 b(v_2);
			if (vec2_evaluation(a.position, b.position)) {
				if (vec2_evaluation(a.size, b.size)) {
					return true;
				}
			}
			return false;
		}
		case Variant::TRANSFORM2D: {
			const Transform2D a(v_1);
			const Transform2D b(v_2);
			if (vec2_evaluation(a.elements[0], b.elements[0])) {
				if (vec2_evaluation(a.elements[1], b.elements[1])) {
					if (vec2_evaluation(a.elements[2], b.elements[2])) {
						return true;
					}
				}
			}
			return false;
		}
		case Variant::VECTOR3: {
			return vec3_evaluation(v_1, v_2);
		}
		case Variant::QUAT: {
			const Quat a = v_1;
			const Quat b = v_2;
			const Quat r(a - b); // Element wise subtraction.
			return (r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w) <= (comparison_float_tolerance * comparison_float_tolerance);
		}
		case Variant::PLANE: {
			const Plane a(v_1);
			const Plane b(v_2);
			if (ABS(a.d - b.d) <= comparison_float_tolerance) {
				if (vec3_evaluation(a.normal, b.normal)) {
					return true;
				}
			}
			return false;
		}
		case Variant::AABB: {
			const AABB a(v_1);
			const AABB b(v_2);
			if (vec3_evaluation(a.position, b.position)) {
				if (vec3_evaluation(a.size, b.size)) {
					return true;
				}
			}
			return false;
		}
		case Variant::BASIS: {
			const Basis a = v_1;
			const Basis b = v_2;
			if (vec3_evaluation(a.elements[0], b.elements[0])) {
				if (vec3_evaluation(a.elements[1], b.elements[1])) {
					if (vec3_evaluation(a.elements[2], b.elements[2])) {
						return true;
					}
				}
			}
			return false;
		}
		case Variant::TRANSFORM: {
			const Transform a = v_1;
			const Transform b = v_2;
			if (vec3_evaluation(a.origin, b.origin)) {
				if (vec3_evaluation(a.basis.elements[0], b.basis.elements[0])) {
					if (vec3_evaluation(a.basis.elements[1], b.basis.elements[1])) {
						if (vec3_evaluation(a.basis.elements[2], b.basis.elements[2])) {
							return true;
						}
					}
				}
			}
			return false;
		}
		case Variant::ARRAY: {
			const Array a = v_1;
			const Array b = v_2;
			if (a.size() != b.size()) {
				return false;
			}
			for (int i = 0; i < a.size(); i += 1) {
				if (synchronizer_variant_evaluation(a[i], b[i]) == false) {
					return false;
				}
			}
			return true;
		}
		case Variant::DICTIONARY: {
			const Dictionary a = v_1;
			const Dictionary b = v_2;

			if (a.size() != b.size()) {
				return false;
			}

			List<Variant> l;
			a.get_key_list(&l);

			for (const List<Variant>::Element *key = l.front(); key; key = key->next()) {
				if (b.has(key->get()) == false) {
					return false;
				}

				if (synchronizer_variant_evaluation(
							a.get(key->get(), Variant()),
							b.get(key->get(), Variant())) == false) {
					return false;
				}
			}

			return true;
		}
		default:
			return v_1 == v_2;
	}
}

bool SceneSynchronizer::is_client() const {
	return synchronizer_type == SYNCHRONIZER_TYPE_CLIENT;
}

void SceneSynchronizer::validate_nodes() {
	LocalVector<NodeData *> null_objects;

	for (uint32_t i = 0; i < node_data.size(); i += 1) {
		if (ObjectDB::get_instance(node_data[i]->instance_id) == nullptr) {
			null_objects.push_back(node_data[i]);
		}
	}

	// Removes the null objects.
	for (uint32_t i = 0; i < null_objects.size(); i += 1) {
		if (null_objects[i]->controlled_by) {
			null_objects[i]->controlled_by->controlled_nodes.erase(null_objects[i]);
			null_objects[i]->controlled_by = nullptr;
		}

		if (null_objects[i]->is_controller) {
			peer_dirty = true;
		}

		synchronizer->on_node_removed(null_objects[i]);

		node_data.erase(null_objects[i]);
		controllers_node_data.erase(null_objects[i]);
		global_nodes_node_data.erase(null_objects[i]);

		memdelete(null_objects[i]);
	}
}

SceneSynchronizer::NodeData *SceneSynchronizer::get_node_data(ObjectID p_object_id) const {
	for (uint32_t i = 0; i < node_data.size(); i += 1) {
		if (node_data[i]->instance_id == p_object_id) {
			return node_data[i];
		}
	}
	return nullptr;
}

uint32_t SceneSynchronizer::find_global_node(ObjectID p_object_id) const {
	for (uint32_t i = 0; i < global_nodes_node_data.size(); i += 1) {
		if (global_nodes_node_data[i]->instance_id == p_object_id) {
			return i;
		}
	}
	return UINT32_MAX;
}

SceneSynchronizer::NodeData *SceneSynchronizer::get_controller_node_data(ControllerID p_controller_id) const {
	for (uint32_t i = 0; i < controllers_node_data.size(); i += 1) {
		if (controllers_node_data[i]->instance_id == p_controller_id) {
			return controllers_node_data[i];
		}
	}
	return nullptr;
}

void SceneSynchronizer::process() {
	validate_nodes();
	synchronizer->process();
}

void SceneSynchronizer::pull_node_changes(NodeData *p_node_data) {
	Node *node = p_node_data->node;
	const int var_count = p_node_data->vars.size();
	VarData *object_vars = p_node_data->vars.ptrw();

	for (int i = 0; i < var_count; i += 1) {
		if (object_vars[i].enabled == false) {
			continue;
		}

		const Variant old_val = object_vars[i].var.value;
		const Variant new_val = node->get(object_vars[i].var.name);

		if (!synchronizer_variant_evaluation(old_val, new_val)) {
			object_vars[i].var.value = new_val.duplicate(true);
			node->emit_signal(get_changed_event_name(object_vars[i].var.name));
			synchronizer->on_variable_changed(p_node_data, object_vars[i].var.name);
		}
	}
}

Synchronizer::Synchronizer(SceneSynchronizer *p_node) :
		scene_synchronizer(p_node) {
}

Synchronizer::~Synchronizer() {
}

NoNetSynchronizer::NoNetSynchronizer(SceneSynchronizer *p_node) :
		Synchronizer(p_node) {}

void NoNetSynchronizer::clear() {
}

void NoNetSynchronizer::process() {
	const real_t delta = scene_synchronizer->get_physics_process_delta_time();

	// Process the scene
	for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
		nd->process(delta);
	}

	// Process the controllers_node_data
	for (uint32_t i = 0; i < scene_synchronizer->controllers_node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->controllers_node_data[i];
		static_cast<NetworkedController *>(nd->node)->get_nonet_controller()->process(delta);
	}

	// Pull the changes.
	for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
		scene_synchronizer->pull_node_changes(nd);
	}
}

void NoNetSynchronizer::receive_snapshot(Variant _p_snapshot) {
}

ServerSynchronizer::ServerSynchronizer(SceneSynchronizer *p_node) :
		Synchronizer(p_node) {}

void ServerSynchronizer::clear() {
	state_notifier_timer = 0.0;
	changes.clear();
}

void ServerSynchronizer::process() {
	const real_t delta = scene_synchronizer->get_physics_process_delta_time();

	// Process the scene
	for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
		nd->process(delta);
	}

	// Process the controllers_node_data
	for (uint32_t i = 0; i < scene_synchronizer->controllers_node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->controllers_node_data[i];
		static_cast<NetworkedController *>(nd->node)->get_server_controller()->process(delta);
	}

	// Pull the changes.
	for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
		SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
		scene_synchronizer->pull_node_changes(nd);
	}

	process_snapshot_notificator(delta);
}

void ServerSynchronizer::receive_snapshot(Variant _p_snapshot) {
	// Unreachable
	CRASH_NOW();
}

void ServerSynchronizer::on_node_added(SceneSynchronizer::NodeData *p_node_data) {
#ifdef DEBUG_ENABLED
	// Can't happen on server
	CRASH_COND(scene_synchronizer->is_recovered());
#endif
	Change *c = changes.lookup_ptr(p_node_data->instance_id);
	if (c) {
		c->not_known_before = true;
	} else {
		Change change;
		change.not_known_before = true;
		changes.set(p_node_data->instance_id, change);
	}
}

void ServerSynchronizer::on_variable_added(SceneSynchronizer::NodeData *p_node_data, StringName p_var_name) {
#ifdef DEBUG_ENABLED
	// Can't happen on server
	CRASH_COND(scene_synchronizer->is_recovered());
#endif
	Change *c = changes.lookup_ptr(p_node_data->instance_id);
	if (c) {
		c->vars.insert(p_var_name);
		c->uknown_vars.insert(p_var_name);
	} else {
		Change change;
		change.vars.insert(p_var_name);
		change.uknown_vars.insert(p_var_name);
		changes.set(p_node_data->instance_id, change);
	}
}

void ServerSynchronizer::on_variable_changed(SceneSynchronizer::NodeData *p_node_data, StringName p_var_name) {
#ifdef DEBUG_ENABLED
	// Can't happen on server
	CRASH_COND(scene_synchronizer->is_recovered());
#endif
	Change *c = changes.lookup_ptr(p_node_data->instance_id);
	if (c) {
		c->vars.insert(p_var_name);
	} else {
		Change change;
		change.vars.insert(p_var_name);
		changes.set(p_node_data->instance_id, change);
	}
}

void ServerSynchronizer::process_snapshot_notificator(real_t p_delta) {
	if (scene_synchronizer->peer_data.empty()) {
		// No one is listening.
		return;
	}

	// Notify the state if needed
	state_notifier_timer += p_delta;
	const bool notify_state = state_notifier_timer >= scene_synchronizer->get_server_notify_state_interval();

	if (notify_state) {
		state_notifier_timer = 0.0;
	}

	scene_synchronizer->update_peers();

	Vector<Variant> full_global_nodes_snapshot;
	Vector<Variant> delta_global_nodes_snapshot;
	for (
			OAHashMap<int, SceneSynchronizer::PeerData>::Iterator peer_it = scene_synchronizer->peer_data.iter();
			peer_it.valid;
			peer_it = scene_synchronizer->peer_data.next_iter(peer_it)) {
		if (peer_it.value->force_notify_snapshot == false && notify_state == false) {
			continue;
		}

		peer_it.value->force_notify_snapshot = false;

		// TODO improve the controller lookup.
		SceneSynchronizer::NodeData *nd = scene_synchronizer->get_controller_node_data(peer_it.value->controller_id);
		// TODO well that's not really true.. I may have peers that doesn't have controllers_node_data in a
		// certain moment. Please improve this mechanism trying to just use the
		// node->get_network_master() to get the peer.
		ERR_CONTINUE_MSG(nd == nullptr, "This should never happen. Likely there is a bug.");

		NetworkedController *controller = static_cast<NetworkedController *>(nd->node);
		if (unlikely(controller->is_enabled() == false)) {
			continue;
		}

		Vector<Variant> snap;
		if (peer_it.value->need_full_snapshot) {
			peer_it.value->need_full_snapshot = false;
			if (full_global_nodes_snapshot.size() == 0) {
				full_global_nodes_snapshot = global_nodes_generate_snapshot(true);
			}
			snap = full_global_nodes_snapshot;
			controller_generate_snapshot(nd, true, snap);
		} else {
			if (delta_global_nodes_snapshot.size() == 0) {
				delta_global_nodes_snapshot = global_nodes_generate_snapshot(false);
			}
			snap = delta_global_nodes_snapshot;
			controller_generate_snapshot(nd, false, snap);
		}

		controller->get_server_controller()->notify_send_state();
		scene_synchronizer->rpc_id(*peer_it.key, "_rpc_send_state", snap);
	}

	if (notify_state) {
		// The state got notified, mark this as checkpoint so the next state
		// will contains only the changed things.
		changes.clear();
	}
}

Vector<Variant> ServerSynchronizer::global_nodes_generate_snapshot(bool p_force_full_snapshot) const {
	Vector<Variant> snapshot_data;

	for (uint32_t i = 0; i < scene_synchronizer->global_nodes_node_data.size(); i += 1) {
		const SceneSynchronizer::NodeData *node_data = scene_synchronizer->global_nodes_node_data[i];
		generate_snapshot_node_data(node_data, p_force_full_snapshot, snapshot_data);
	}

	return snapshot_data;
}

void ServerSynchronizer::controller_generate_snapshot(
		const SceneSynchronizer::NodeData *p_node_data,
		bool p_force_full_snapshot,
		Vector<Variant> &r_snapshot_result) const {
	CRASH_COND(p_node_data->is_controller == false);

	generate_snapshot_node_data(
			p_node_data,
			p_force_full_snapshot,
			r_snapshot_result);

	for (uint32_t i = 0; i < p_node_data->controlled_nodes.size(); i += 1) {
		generate_snapshot_node_data(
				p_node_data->controlled_nodes[i],
				p_force_full_snapshot,
				r_snapshot_result);
	}
}

void ServerSynchronizer::generate_snapshot_node_data(
		const SceneSynchronizer::NodeData *p_node_data,
		bool p_force_full_snapshot,
		Vector<Variant> &r_snapshot_data) const {
	// The packet data is an array that contains the informations to update the
	// client snapshot.
	//
	// It's composed as follows:
	//  [NODE, VARIABLE, Value, VARIABLE, Value, VARIABLE, value, NIL,
	//  NODE, INPUT ID, VARIABLE, Value, VARIABLE, Value, NIL,
	//  NODE, VARIABLE, Value, VARIABLE, Value, NIL]
	//
	// Each node ends with a NIL, and the NODE and the VARIABLE are special:
	// - NODE, can be an array of two variables [Node ID, NodePath] or directly
	//         a Node ID. Obviously the array is sent only the first time.
	// - INPUT ID, this is optional and is used only when the node is a controller.
	// - VARIABLE, can be an array with the ID and the variable name, or just
	//              the ID; similarly as is for the NODE the array is send only
	//              the first time.

	if (p_node_data->node == nullptr || p_node_data->node->is_inside_tree() == false) {
		return;
	}

	const Change *change = changes.lookup_ptr(p_node_data->instance_id);

	// Insert NODE DATA.
	Variant snap_node_data;
	if (p_force_full_snapshot || (change != nullptr && change->not_known_before)) {
		Vector<Variant> _snap_node_data;
		_snap_node_data.resize(2);
		_snap_node_data.write[0] = p_node_data->id;
		_snap_node_data.write[1] = p_node_data->node->get_path();
		snap_node_data = _snap_node_data;
	} else {
		// This node is already known on clients, just set the node ID.
		snap_node_data = p_node_data->id;
	}

	const bool node_has_changes = p_force_full_snapshot || (change != nullptr && change->vars.empty() == false);

	if (p_node_data->is_controller) {
		NetworkedController *controller = static_cast<NetworkedController *>(p_node_data->node);

		// TODO make sure to skip un-active controllers_node_data.
		//  This may no more needed, since the interpolator got integrated and
		//  the only time the controller is sync is when it's needed.
		if (likely(controller->get_current_input_id() != UINT32_MAX)) {
			// This is a controller, always sync it.
			r_snapshot_data.push_back(snap_node_data);
			r_snapshot_data.push_back(controller->get_current_input_id());
		} else {
			// The first ID id is not yet arrived, so just skip this node.
			return;
		}
	} else {
		if (node_has_changes) {
			r_snapshot_data.push_back(snap_node_data);
		} else {
			// It has no changes, skip this node.
			return;
		}
	}

	if (node_has_changes) {
		// Insert the node variables.
		const int size = p_node_data->vars.size();
		const SceneSynchronizer::VarData *vars = &p_node_data->vars[0];
		for (int i = 0; i < size; i += 1) {
			if (vars[i].enabled == false) {
				continue;
			}

			if (p_force_full_snapshot == false && change->vars.has(vars[i].var.name) == false) {
				// This is a delta snapshot and this variable is the same as
				// before. Skip it.
				continue;
			}

			Variant var_info;
			if (p_force_full_snapshot || change->uknown_vars.has(vars[i].var.name)) {
				Vector<Variant> _var_info;
				_var_info.resize(2);
				_var_info.write[0] = vars[i].id;
				_var_info.write[1] = vars[i].var.name;
				var_info = _var_info;
			} else {
				var_info = vars[i].id;
			}

			r_snapshot_data.push_back(var_info);
			r_snapshot_data.push_back(vars[i].var.value);
		}
	}

	// Insert NIL.
	r_snapshot_data.push_back(Variant());
}

ClientSynchronizer::ClientSynchronizer(SceneSynchronizer *p_node) :
		Synchronizer(p_node) {
	clear();
}

void ClientSynchronizer::clear() {
	node_id_map.clear();
	node_paths.clear();
	last_received_snapshot.input_id = UINT32_MAX;
	last_received_snapshot.node_vars.clear();
	client_snapshots.clear();
	server_snapshots.clear();
}

void ClientSynchronizer::process() {
	if (player_controller_node_data == nullptr) {
		// No player controller, nothing to do.
		return;
	}

	const real_t delta = scene_synchronizer->get_physics_process_delta_time();
	const real_t iteration_per_second = Engine::get_singleton()->get_iterations_per_second();

	NetworkedController *controller = static_cast<NetworkedController *>(player_controller_node_data->node);
	PlayerController *player_controller = controller->get_player_controller();

	// Reset this here, so even when `sub_ticks` is zero (and it's not
	// updated due to process is not called), we can still have the corect
	// data.
	controller->player_set_has_new_input(false);

	// Due to some lag we may want to speed up the input_packet
	// generation, for this reason here I'm performing a sub tick.
	//
	// keep in mind that we are just pretending that the time
	// is advancing faster, for this reason we are still using
	// `delta` to step the controllers_node_data.
	//
	// The dolls may want to speed up too, so to consume the inputs faster
	// and get back in time with the server.
	int sub_ticks = player_controller->calculates_sub_ticks(delta, iteration_per_second);

	while (sub_ticks > 0) {
		// Process the scene.
		for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
			SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
			nd->process(delta);
		}

		// Process the player controllers_node_data.
		player_controller->process(delta);

		// Pull the changes.
		for (uint32_t i = 0; i < scene_synchronizer->node_data.size(); i += 1) {
			SceneSynchronizer::NodeData *nd = scene_synchronizer->node_data[i];
			scene_synchronizer->pull_node_changes(nd);
		}

		if (controller->player_has_new_input()) {
			store_snapshot();
		}

		sub_ticks -= 1;
	}

	scene_synchronizer->recover_in_progress = true;
	process_controllers_recovery(delta);
	scene_synchronizer->recover_in_progress = false;
}

void ClientSynchronizer::receive_snapshot(Variant p_snapshot) {
	// The received snapshot is parsed and stored into the `last_received_snapshot`
	// that contains always the last received snapshot.
	// Later, the snapshot is stored into the server queue.
	// In this way, we are free to pop snapshot from the queue without wondering
	// about losing the data. Indeed the received snapshot is just and
	// incremental update so the last received data is always needed to fully
	// reconstruct it.

	// Parse server snapshot.
	const bool success = parse_snapshot(p_snapshot);

	if (success == false) {
		return;
	}

	// Finalize data.

	store_controllers_snapshot(
			last_received_snapshot,
			server_snapshots);
}

void ClientSynchronizer::on_node_added(SceneSynchronizer::NodeData *p_node_data) {
	if (p_node_data->is_controller == false) {
		// Nothing to do.
		return;
	}
	ERR_FAIL_COND_MSG(player_controller_node_data != nullptr, "Only one player controller is supported, at the moment.");
	if (static_cast<NetworkedController *>(p_node_data->node)->is_player_controller()) {
		player_controller_node_data = p_node_data;
	}
}

void ClientSynchronizer::on_node_removed(SceneSynchronizer::NodeData *p_node_data) {
	if (player_controller_node_data == p_node_data) {
		player_controller_node_data = nullptr;
	}
}

void ClientSynchronizer::store_snapshot() {
	NetworkedController *controller = static_cast<NetworkedController *>(player_controller_node_data->node);

	if (client_snapshots.size() > 0 && controller->get_current_input_id() <= client_snapshots.back().input_id) {
		NET_DEBUG_ERR("During snapshot creation, for controller " + controller->get_path() + ", was found an ID for an older snapshots. New input ID: " + itos(controller->get_current_input_id()) + " Last saved snapshot input ID: " + itos(client_snapshots.back().input_id) + ". This snapshot is not stored.");
		return;
	}

	client_snapshots.push_back(SceneSynchronizer::Snapshot());

	SceneSynchronizer::Snapshot &snap = client_snapshots.back();
	snap.input_id = controller->get_current_input_id();

	// Store the state of all the global nodes.
	for (uint32_t i = 0; i < scene_synchronizer->global_nodes_node_data.size(); i += 1) {
		const SceneSynchronizer::NodeData *node_data = scene_synchronizer->global_nodes_node_data[i];
		snap.node_vars.set(node_data->instance_id, node_data->vars);
	}

	// Store the controller state.
	snap.node_vars.set(player_controller_node_data->instance_id, player_controller_node_data->vars);

	// Store the controlled node state.
	for (uint32_t i = 0; i < player_controller_node_data->controlled_nodes.size(); i += 1) {
		const SceneSynchronizer::NodeData *node_data = player_controller_node_data->controlled_nodes[i];
		snap.node_vars.set(node_data->instance_id, node_data->vars);
	}
}

void ClientSynchronizer::store_controllers_snapshot(
		const SceneSynchronizer::Snapshot &p_snapshot,
		std::deque<SceneSynchronizer::Snapshot> &r_snapshot_storage) {
	// Put the parsed snapshot into the queue.

	if (p_snapshot.input_id == UINT32_MAX) {
		// The snapshot doesn't have any info for this controller; Skip it.
		return;
	}

	if (r_snapshot_storage.empty() == false) {
		// Make sure the snapshots are stored in order.
		const uint32_t last_stored_input_id = r_snapshot_storage.back().input_id;
		if (p_snapshot.input_id == last_stored_input_id) {
			// Update the snapshot.
			r_snapshot_storage.back() = p_snapshot;
			return;
		} else {
			ERR_FAIL_COND_MSG(p_snapshot.input_id < last_stored_input_id, "This snapshot (with ID: " + itos(p_snapshot.input_id) + ") is not expected because the last stored id is: " + itos(last_stored_input_id));
		}
	}

	r_snapshot_storage.push_back(p_snapshot);
}

void ClientSynchronizer::process_controllers_recovery(real_t p_delta) {
	// The client is responsible to recover only its local controller, while all
	// the other controllers_node_data (dolls) have their state interpolated. There is
	// no need to check the correctness of the doll state nor the needs to
	// rewind those.
	//
	// The scene, (global nodes), are always in sync with the reference frame
	// of the client.

	NetworkedController *controller = static_cast<NetworkedController *>(player_controller_node_data->node);
	PlayerController *player_controller = controller->get_player_controller();

	// --- Phase one: find the snapshot to check. ---
	if (server_snapshots.empty()) {
		// No snapshots to recover for this controller. Nothing to do.
		return;
	}

#ifdef DEBUG_ENABLED
	if (client_snapshots.empty() == false) {
		// The SceneSynchronizer and the PlayerController are always in sync.
		CRASH_COND(client_snapshots.back().input_id != player_controller->last_known_input());
	}
#endif

	// Find the best recoverable input_id.
	uint32_t checkable_input_id = UINT32_MAX;
	// Find the best snapshot to recover from the one already
	// processed.
	if (client_snapshots.empty() == false) {
		for (
				auto s_snap = server_snapshots.rbegin();
				checkable_input_id == UINT32_MAX && s_snap != server_snapshots.rend();
				++s_snap) {
			for (auto c_snap = client_snapshots.begin(); c_snap != client_snapshots.end(); ++c_snap) {
				if (c_snap->input_id == s_snap->input_id) {
					// Server snapshot also found on client, can be checked.
					checkable_input_id = c_snap->input_id;
					break;
				}
			}
		}
	}

	if (checkable_input_id == UINT32_MAX) {
		// No snapshot found, nothing to do.
		return;
	}

#ifdef DEBUG_ENABLED
	// Unreachable cause the above check
	CRASH_COND(server_snapshots.empty());
	CRASH_COND(client_snapshots.empty());
#endif

	// Drop all the old server snapshots until the one that we need.
	while (server_snapshots.front().input_id < checkable_input_id) {
		server_snapshots.pop_front();
	}

	// Drop all the old client snapshots until the one that we need.
	while (client_snapshots.front().input_id < checkable_input_id) {
		client_snapshots.pop_front();
	}

#ifdef DEBUG_ENABLED
	// These are unreachable at this point.
	CRASH_COND(server_snapshots.empty());
	CRASH_COND(server_snapshots.front().input_id != checkable_input_id);

	// This is unreachable, because we store all the client shapshots
	// each time a new input is processed. Since the `checkable_input_id`
	// is taken by reading the processed doll inputs, it's guaranteed
	// that here the snapshot exists.
	CRASH_COND(client_snapshots.empty());
	CRASH_COND(client_snapshots.front().input_id != checkable_input_id);
#endif

	// --- Phase two: compare the server snapshot with the client snapshot. ---
	bool need_recover = false;
	bool recover_controller = false;
	LocalVector<SceneSynchronizer::NodeData *> nodes_to_recover;
	LocalVector<SceneSynchronizer::PostponedRecover> postponed_recover;

	nodes_to_recover.reserve(server_snapshots.front().node_vars.get_num_elements());
	for (
			OAHashMap<ObjectID, Vector<SceneSynchronizer::VarData>>::Iterator s_snap_it = server_snapshots.front().node_vars.iter();
			s_snap_it.valid;
			s_snap_it = server_snapshots.front().node_vars.next_iter(s_snap_it)) {
		SceneSynchronizer::NodeData *rew_node_data = scene_synchronizer->get_node_data(*s_snap_it.key);
		if (rew_node_data == nullptr) {
			continue;
		}

		bool recover_this_node = false;
		const Vector<SceneSynchronizer::VarData> *c_vars = client_snapshots.front().node_vars.lookup_ptr(*s_snap_it.key);
		if (c_vars == nullptr) {
			NET_DEBUG_PRINT("Rewind is needed because the client snapshot doesn't contain this node: " + rew_node_data->node->get_path());
			recover_this_node = true;
		} else {
			SceneSynchronizer::PostponedRecover rec;

			const bool different = compare_vars(
					rew_node_data,
					*s_snap_it.value,
					*c_vars,
					rec.vars);

			if (different) {
				NET_DEBUG_PRINT("Rewind is needed because the node on client is different: " + rew_node_data->node->get_path());
				recover_this_node = true;
			} else if (rec.vars.size() > 0) {
				rec.node_data = rew_node_data;
				postponed_recover.push_back(rec);
			}
		}

		if (recover_this_node) {
			need_recover = true;
			if (rew_node_data->controlled_by != nullptr ||
					rew_node_data->is_controller) {
				// Controller node.
				recover_controller = true;
			} else {
				nodes_to_recover.push_back(rew_node_data);
			}
		}
	}

	// Popout the client snapshot.
	client_snapshots.pop_front();

	// --- Phase three: recover and reply. ---

	if (need_recover) {
		NET_DEBUG_PRINT("Recover input: " + itos(checkable_input_id) + " - Last input: " + itos(player_controller->get_stored_input_id(-1)));

		if (recover_controller) {
			// Put the controlled and the controllers_node_data into the nodes to
			// rewind.
			// Note, the controller stuffs are added here to ensure that if the
			// controller need a recover, all its nodes are added; no matter
			// at which point the difference is found.
			nodes_to_recover.reserve(
					nodes_to_recover.size() +
					player_controller_node_data->controlled_nodes.size() +
					1);

			nodes_to_recover.push_back(player_controller_node_data);

			for (
					uint32_t y = 0;
					y < player_controller_node_data->controlled_nodes.size();
					y += 1) {
				nodes_to_recover.push_back(player_controller_node_data->controlled_nodes[y]);
			}
		}

		// Apply the server snapshot so to go back in time till that moment,
		// so to be able to correctly reply the movements.
		scene_synchronizer->reset_in_progress = true;
		for (
				uint32_t i = 0;
				i < nodes_to_recover.size();
				i += 1) {
			Node *node = nodes_to_recover[i]->node;

			const Vector<SceneSynchronizer::VarData> *s_vars = server_snapshots.front().node_vars.lookup_ptr(nodes_to_recover[i]->instance_id);
			if (s_vars == nullptr) {
				NET_DEBUG_WARN("The node: " + nodes_to_recover[i]->node->get_path() + " was not found on the server snapshot, this is not supposed to happen a lot.");
				continue;
			}
			const SceneSynchronizer::VarData *s_vars_ptr = s_vars->ptr();

			NET_DEBUG_PRINT("Full reset node: " + node->get_path());
			SceneSynchronizer::VarData *nodes_to_recover_vars_ptr = nodes_to_recover[i]->vars.ptrw();
			for (int v = 0; v < s_vars->size(); v += 1) {
				node->set(s_vars_ptr[v].var.name, s_vars_ptr[v].var.value);

				// Set the value on the synchronizer too.
				const int rew_var_index = nodes_to_recover[i]->vars.find(s_vars_ptr[v].var.name);
				// Unreachable, because when the snapshot is received the
				// algorithm make sure the `scene_synchronizer` is traking the
				// variable.
				CRASH_COND(rew_var_index <= -1);

				NET_DEBUG_PRINT(" |- Variable: " + s_vars_ptr[v].var.name + " New value: " + s_vars_ptr[v].var.value);

				nodes_to_recover_vars_ptr[rew_var_index].var.value = s_vars_ptr[v].var.value.duplicate(true);

				node->emit_signal(
						scene_synchronizer->get_changed_event_name(
								s_vars_ptr[v].var.name));
			}
		}
		scene_synchronizer->reset_in_progress = false;

		// Rewind phase.

		scene_synchronizer->rewinding_in_progress = true;
		const int remaining_inputs = player_controller->notify_input_checked(checkable_input_id);
#ifdef DEBUG_ENABLED
		// Unreachable because the SceneSynchronizer and the PlayerController
		// have the same stored data at this point.
		CRASH_COND(client_snapshots.size() != size_t(remaining_inputs));
#endif

		bool has_next = false;
		for (int i = 0; i < remaining_inputs; i += 1) {
			// Step 1 -- Process the nodes into the scene that need to be
			// processed.
			for (
					uint32_t r = 0;
					r < nodes_to_recover.size();
					r += 1) {
				nodes_to_recover[r]->process(p_delta);
#ifdef DEBUG_ENABLED
				if (nodes_to_recover[r]->functions.size()) {
					NET_DEBUG_PRINT("Rewind, processed node: " + nodes_to_recover[r]->node->get_path());
				}
#endif
			}

			if (recover_controller) {
				// Step 2 -- Process the controller.
				has_next = controller->process_instant(i, p_delta);
				NET_DEBUG_PRINT("Rewind, processed controller: " + controller->get_path());
			}

			// Step 3 -- Pull node changes and Update snapshots.
			for (
					uint32_t r = 0;
					r < nodes_to_recover.size();
					r += 1) {
				scene_synchronizer->pull_node_changes(nodes_to_recover[r]);

				// Update client snapshot.
				client_snapshots[i].node_vars.set(nodes_to_recover[r]->instance_id, nodes_to_recover[r]->vars);
			}
		}

#ifdef DEBUG_ENABLED
		// Unreachable because the above loop consume all instants.
		CRASH_COND(has_next);
#endif

		scene_synchronizer->rewinding_in_progress = false;
	} else {
		// Apply found differences without rewind.
		scene_synchronizer->reset_in_progress = true;
		for (
				uint32_t i = 0;
				i < postponed_recover.size();
				i += 1) {
			SceneSynchronizer::NodeData *rew_node_data = postponed_recover[i].node_data;
			Node *node = rew_node_data->node;
			const SceneSynchronizer::Var *vars = postponed_recover[i].vars.ptr();

			NET_DEBUG_PRINT("[Snapshot partial reset] Node: " + node->get_path());

			{
				SceneSynchronizer::VarData *rew_node_data_vars_ptr = rew_node_data->vars.ptrw();
				for (int v = 0; v < postponed_recover[i].vars.size(); v += 1) {
					node->set(vars[v].name, vars[v].value);

					// Set the value on the synchronizer too.
					const int rew_var_index = rew_node_data->vars.find(vars[v].name);
					// Unreachable, because when the snapshot is received the
					// algorithm make sure the `scene_synchronizer` is traking the
					// variable.
					CRASH_COND(rew_var_index <= -1);

					rew_node_data_vars_ptr[rew_var_index].var.value = vars[v].value.duplicate(true);

					NET_DEBUG_PRINT(" |- Variable: " + vars[v].name + "; value: " + vars[v].value);
					node->emit_signal(scene_synchronizer->get_changed_event_name(vars[v].name));
				}
			}

			// Update the last client snapshot.
			if (client_snapshots.empty() == false) {
				client_snapshots.back().node_vars.set(rew_node_data->instance_id, rew_node_data->vars);
			}
		}
		scene_synchronizer->reset_in_progress = false;

		player_controller->notify_input_checked(checkable_input_id);
	}

	// Popout the server snapshot.
	server_snapshots.pop_front();
}

bool ClientSynchronizer::parse_snapshot(Variant p_snapshot) {
	// The packet data is an array that contains the informations to update the
	// client snapshot.
	//
	// It's composed as follows:
	//  [NODE, VARIABLE, Value, VARIABLE, Value, VARIABLE, value, NIL,
	//  NODE, INPUT ID, VARIABLE, Value, VARIABLE, Value, NIL,
	//  NODE, VARIABLE, Value, VARIABLE, Value, NIL]
	//
	// Each node ends with a NIL, and the NODE and the VARIABLE are special:
	// - NODE, can be an array of two variables [Node ID, NodePath] or directly
	//         a Node ID. Obviously the array is sent only the first time.
	// - INPUT ID, this is optional and is used only when the node is a controller.
	// - VARIABLE, can be an array with the ID and the variable name, or just
	//              the ID; similarly as is for the NODE the array is send only
	//              the first time.

	need_full_snapshot_notified = false;

	ERR_FAIL_COND_V_MSG(
			player_controller_node_data == nullptr,
			false,
			"Is not possible to receive server snapshots if you are not tracking any NetController.");
	ERR_FAIL_COND_V(!p_snapshot.is_array(), false);

	const Vector<Variant> raw_snapshot = p_snapshot;
	const Variant *raw_snapshot_ptr = raw_snapshot.ptr();

	Node *node = nullptr;
	SceneSynchronizer::NodeData *synchronizer_node_data = nullptr;
	Vector<SceneSynchronizer::VarData> *server_snapshot_node_data = nullptr;
	StringName variable_name;
	int server_snap_variable_index = -1;

	last_received_snapshot.input_id = UINT32_MAX;

	for (int snap_data_index = 0; snap_data_index < raw_snapshot.size(); snap_data_index += 1) {
		const Variant v = raw_snapshot_ptr[snap_data_index];
		if (node == nullptr) {
			// Node is null so we expect `v` has the node info.

			uint32_t node_id(0);

			if (v.is_array()) {
				// Node info are in verbose form, extract it.

				const Vector<Variant> node_data = v;
				ERR_FAIL_COND_V(node_data.size() != 2, false);
				ERR_FAIL_COND_V(node_data[0].get_type() != Variant::INT, false);
				ERR_FAIL_COND_V(node_data[1].get_type() != Variant::NODE_PATH, false);

				node_id = node_data[0];
				const NodePath node_path = node_data[1];

				// Associate the ID with the path.
				node_paths.set(node_id, node_path);

				node = scene_synchronizer->get_tree()->get_root()->get_node(node_path);

			} else if (v.get_type() == Variant::INT) {
				// Node info are in short form.

				node_id = v;

				const ObjectID *object_id = node_id_map.lookup_ptr(node_id);
				if (object_id != nullptr) {
					Object *const obj = ObjectDB::get_instance(*object_id);
					node = Object::cast_to<Node>(obj);
					if (node == nullptr) {
						// This node doesn't exist anymore.
						node_id_map.remove(node_id);
					}
				}

				if (node == nullptr) {
					// The node instance for this node ID was not found, try
					// to find it now.

					const NodePath *node_path = node_paths.lookup_ptr(node_id);
					if (node_path == nullptr) {
						NET_DEBUG_PRINT("The node with ID `" + itos(node_id) + "` is not know by this peer, this is not supposed to happen.");
						notify_server_full_snapshot_is_needed();
					} else {
						node = scene_synchronizer->get_tree()->get_root()->get_node(*node_path);
					}
				}
			} else {
				// The arrived snapshot does't seems to be in the expected form.
				ERR_FAIL_V_MSG(false, "Snapshot is corrupted.");
			}

			synchronizer_node_data = node ? scene_synchronizer->get_node_data(node->get_instance_id()) : nullptr;
			if (synchronizer_node_data == nullptr) {
				// This node does't exist; skip it entirely.
				for (snap_data_index += 1; snap_data_index < raw_snapshot.size(); snap_data_index += 1) {
					if (raw_snapshot_ptr[snap_data_index].get_type() == Variant::NIL) {
						break;
					}
				}
				ERR_CONTINUE_MSG(true, "This node doesn't exist on this client: " + itos(node_id));
			} else {
				// The node is found, make sure to update the instance ID in
				// case it changed or it doesn't exist.
				node_id_map.set(node_id, node->get_instance_id());
			}

			// Update the node ID created on the server.
			synchronizer_node_data->id = node_id;

			// Make sure this node is part of the server node too.
			server_snapshot_node_data = last_received_snapshot.node_vars.lookup_ptr(node->get_instance_id());
			if (server_snapshot_node_data == nullptr) {
				last_received_snapshot.node_vars.set(
						node->get_instance_id(),
						Vector<SceneSynchronizer::VarData>());
				server_snapshot_node_data = last_received_snapshot.node_vars.lookup_ptr(node->get_instance_id());
			}

			if (synchronizer_node_data->is_controller) {
				// This is a controller, so the next data is the input ID.
				ERR_FAIL_COND_V(snap_data_index + 1 >= raw_snapshot.size(), false);
				snap_data_index += 1;
				const uint32_t input_id = raw_snapshot_ptr[snap_data_index];
				ERR_FAIL_COND_V_MSG(input_id == UINT32_MAX, false, "The server is always able to send input_id, so this snapshot seems corrupted.");

				if (synchronizer_node_data == player_controller_node_data) {
					// This is the main controller, store the input ID.
					last_received_snapshot.input_id = input_id;
				}
			}

		} else if (variable_name == StringName()) {
			// When the node is known and the `variable_name` not, we expect a
			// new variable or the end pf this node data.

			if (v.get_type() == Variant::NIL) {
				// NIL found, so this node is done.
				node = nullptr;
				continue;
			}

			// This is a new variable, so let's take the variable name.

			uint32_t var_id;
			if (v.is_array()) {
				// The variable info are stored in verbose mode.

				const Vector<Variant> var_data = v;
				ERR_FAIL_COND_V(var_data.size() != 2, false);
				ERR_FAIL_COND_V(var_data[0].get_type() != Variant::INT, false);
				ERR_FAIL_COND_V(var_data[1].get_type() != Variant::STRING_NAME, false);

				var_id = var_data[0];
				variable_name = var_data[1];

				const int64_t index = synchronizer_node_data->vars.find(variable_name);

				if (index == -1) {
					// The variable is not known locally, so just add it so
					// to store the variable ID.
					const bool skip_rewinding = false;
					const bool enabled = false;
					synchronizer_node_data->vars
							.push_back(
									SceneSynchronizer::VarData(
											var_id,
											variable_name,
											Variant(),
											skip_rewinding,
											enabled));
				} else {
					// The variable is known, just make sure that it has the
					// same server ID.
					synchronizer_node_data->vars.write[index].id = var_id;
				}
			} else if (v.get_type() == Variant::INT) {
				// The variable is stored in the compact form.

				var_id = v;

				const int64_t index = synchronizer_node_data->find_var_by_id(var_id);
				if (index == -1) {
					NET_DEBUG_PRINT("The var with ID `" + itos(var_id) + "` is not know by this peer, this is not supposed to happen.");

					notify_server_full_snapshot_is_needed();

					// Skip the next data since it should be the value, but we
					// can't store it.
					snap_data_index += 1;
					continue;
				} else {
					variable_name = synchronizer_node_data->vars[index].var.name;
					synchronizer_node_data->vars.write[index].id = var_id;
				}

			} else {
				ERR_FAIL_V_MSG(false, "The snapshot received seems corrupted.");
			}

			server_snap_variable_index = server_snapshot_node_data->find(variable_name);

			if (server_snap_variable_index == -1) {
				// The server snapshot seems not contains this yet.
				server_snap_variable_index = server_snapshot_node_data->size();

				const bool skip_rewinding = false;
				const bool enabled = true;
				server_snapshot_node_data->push_back(
						SceneSynchronizer::VarData(
								var_id,
								variable_name,
								Variant(),
								skip_rewinding,
								enabled));

			} else {
				server_snapshot_node_data->write[server_snap_variable_index].id = var_id;
			}

		} else {
			// The node is known, also the variable name is known, so the value
			// is expected.

			server_snapshot_node_data->write[server_snap_variable_index]
					.var
					.value = v.duplicate(true);

			// Just reset the variable name so we can continue iterate.
			variable_name = StringName();
			server_snap_variable_index = -1;
		}
	}

	// We espect that the player_controller is updated by this new snapshot,
	// so make sure it's done so.
	if (unlikely(last_received_snapshot.input_id == UINT32_MAX)) {
		NET_DEBUG_PRINT("Recovery aborted, the player controller (" + player_controller_node_data->node->get_path() + ") was not part of the received snapshot, probably the server doesn't have important informations for this peer. Snapshot:");
		NET_DEBUG_PRINT(p_snapshot);
		return false;
	} else {
		return true;
	}
}

bool ClientSynchronizer::compare_vars(
		const SceneSynchronizer::NodeData *p_synchronizer_node_data,
		const Vector<SceneSynchronizer::VarData> &p_server_vars,
		const Vector<SceneSynchronizer::VarData> &p_client_vars,
		Vector<SceneSynchronizer::Var> &r_postponed_recover) {
	const SceneSynchronizer::VarData *s_vars = p_server_vars.ptr();
	const SceneSynchronizer::VarData *c_vars = p_client_vars.ptr();

#ifdef DEBUG_ENABLED
	bool diff = false;
#endif

	for (int s_var_index = 0; s_var_index < p_server_vars.size(); s_var_index += 1) {
		const int c_var_index = p_client_vars.find(s_vars[s_var_index].var.name);
		if (c_var_index == -1) {
			// Variable not found, this is considered a difference.
			NET_DEBUG_PRINT("Difference found on the var name `" + s_vars[s_var_index].var.name + "`, it was not found on client snapshot. Server value: `" + s_vars[s_var_index].var.value + "`.");
#ifdef DEBUG_ENABLED
			diff = true;
#else
			return true;
#endif
		} else {
			// Variable found compare.
			const bool different = !scene_synchronizer->synchronizer_variant_evaluation(s_vars[s_var_index].var.value, c_vars[c_var_index].var.value);

			if (different) {
				const int index = p_synchronizer_node_data->vars.find(s_vars[s_var_index].var.name);
				if (index < 0 || p_synchronizer_node_data->vars[index].skip_rewinding == false) {
					// The vars are different.
					NET_DEBUG_PRINT("Difference found on var name `" + s_vars[s_var_index].var.name + "` Server value: `" + s_vars[s_var_index].var.value + "` Client value: `" + c_vars[c_var_index].var.value + "`.");
#ifdef DEBUG_ENABLED
					diff = true;
#else
					return true;
#endif
				} else {
					// The vars are different, but this variable don't what to
					// trigger a rewind.
					r_postponed_recover.push_back(s_vars[s_var_index].var);
				}
			}
		}
	}

#ifdef DEBUG_ENABLED
	return diff;
#else
	// The vars are not different.
	return false;
#endif
}

void ClientSynchronizer::notify_server_full_snapshot_is_needed() {
	if (need_full_snapshot_notified) {
		return;
	}

	// Notify the server that a full snapshot is needed.
	need_full_snapshot_notified = true;
	scene_synchronizer->rpc_id(1, "_rpc_notify_need_full_snapshot");
}
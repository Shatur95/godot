/*************************************************************************/
/*  scene_rewinder.h                                                     */
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

#include "scene/main/node.h"

#include "core/hash_map.h"
#include <deque>

#ifndef SCENE_REWINDER_H
#define SCENE_REWINDER_H

class Rewinder;
class CharacterNetController;

struct VarData {
	uint32_t id;
	StringName name;
	Variant old_val;
	bool enabled;

	VarData();
	VarData(StringName p_name);
	VarData(uint32_t p_id, StringName p_name, Variant p_val, bool p_enabled);

	bool operator==(const VarData &p_other) const;
};

struct NodeData {
	uint32_t id;
	ObjectID instance_id;
	Vector<VarData> vars;

	// This is valid to use only inside the process function.
	Node *cached_node;

	NodeData();
	NodeData(uint32_t p_id, ObjectID p_instance_id);
};

class SceneRewinder : public Node {
	GDCLASS(SceneRewinder, Node);

	friend class Rewinder;
	friend class ServerRewinder;
	friend class ClientRewinder;
	friend class NoNetRewinder;

	real_t server_notify_state_interval;

	Rewinder *rewinder;

	uint32_t node_counter;
	bool generate_id;
	HashMap<ObjectID, NodeData> data;
	Vector<CharacterNetController *> controllers;

public:
	static void _bind_methods();

	virtual void _notification(int p_what);

public:
	SceneRewinder();
	~SceneRewinder();

	void set_server_notify_state_interval(real_t p_interval);
	real_t get_server_notify_state_interval() const;

	void register_variable(Node *p_node, StringName p_variable, StringName p_on_change_notify_to = StringName());
	void unregister_variable(Node *p_node, StringName p_variable);

	String get_changed_event_name(StringName p_variable);

	void track_variable_changes(Node *p_node, StringName p_variable, StringName p_method);
	void untrack_variable_changes(Node *p_node, StringName p_variable, StringName p_method);

	/// Can only be called on the server
	void reset();
	void __reset();
	/// Can only be called on the server
	void clear();
	void __clear();

	void _rpc_send_state(Variant p_snapshot);

private:
	void process();

	void on_peer_connected(int p_peer_id);
	void on_peer_disconnected(int p_peer_id);
};

struct PeerData {
	int peer;
	// List of nodes which the server sent the variable information.
	Vector<uint32_t> nodes_know_variables;

	PeerData();
	PeerData(int p_peer);

	bool operator==(const PeerData &p_other) const;
};

struct Snapshot {
	uint64_t snapshot_id;
	Vector<NodeData> data;
};

class Rewinder {
protected:
	SceneRewinder *scene_rewinder;

public:
	Rewinder(SceneRewinder *p_node);

	virtual void clear() = 0;

	virtual void process(real_t p_delta) = 0;
	virtual void receive_snapshot(Variant p_snapshot) = 0;
};

class NoNetRewinder : public Rewinder {
public:
	NoNetRewinder(SceneRewinder *p_node);

	virtual void clear();

	virtual void process(real_t p_delta);
	virtual void receive_snapshot(Variant p_snapshot);
};

class ServerRewinder : public Rewinder {
	real_t state_notifier_timer;
	Vector<PeerData> peers_data;

public:
	ServerRewinder(SceneRewinder *p_node);

	virtual void clear();

	void on_peer_connected(int p_peer_id);
	void on_peer_disconnected(int p_peer_id);

	Variant generate_snapshot();

	virtual void process(real_t p_delta);
	virtual void receive_snapshot(Variant p_snapshot);
};

class ClientRewinder : public Rewinder {

	HashMap<uint32_t, ObjectID> node_id_map;
	HashMap<uint32_t, NodePath> node_paths;

	std::deque<Snapshot> snapshots;

public:
	ClientRewinder(SceneRewinder *p_node);

	virtual void clear();

	virtual void process(real_t p_delta);
	virtual void receive_snapshot(Variant p_snapshot);
};

#endif

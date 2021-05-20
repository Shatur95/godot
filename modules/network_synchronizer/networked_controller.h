/*************************************************************************/
/*  networked_controller.h                                               */
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

#include "scene/main/node.h"

#include "core/math/transform.h"
#include "core/node_path.h"
#include "input_buffer.h"
#include "interpolator.h"
#include "net_utilities.h"
#include <deque>
#include <vector>

#ifndef NETWORKED_CONTROLLER_H
#define NETWORKED_CONTROLLER_H

struct Controller;
class SceneSynchronizer;

/// The `NetworkedController` is responsible to sync the `Player` inputs between
/// the peers. This allows to control a character, or an object with high precision
/// and replicates that movement on all connected peers.
///
/// The `NetworkedController` will sync inputs, based on those will perform
/// operations.
/// The result of these operations, are guaranteed to be the same accross the
/// peers, if we stay under the assumption that the initial state is the same.
///
/// Is possible to use the `SceneSynchronizer` to keep the state in sync with the
/// peers.
///
// # Implementation details
//
// The `NetworkedController` perform different operations depending where it's
// instantiated.
// The most important part is inside the `PlayerController`, `ServerController`,
// `DollController`, `NoNetController`.
class NetworkedController : public Node {
	GDCLASS(NetworkedController, Node);

public:
	enum ControllerType {
		CONTROLLER_TYPE_NULL,
		CONTROLLER_TYPE_NONETWORK,
		CONTROLLER_TYPE_PLAYER,
		CONTROLLER_TYPE_SERVER,
		CONTROLLER_TYPE_DOLL
	};

private:
	/// The input storage size is used to cap the amount of inputs collected by
	/// the `Master`.
	///
	/// The server sends a message, to all the connected peers, notifing its
	/// status at a fixed interval.
	/// The peers, after receiving this update, removes all the old inputs until
	/// that moment.
	///
	/// If the `input_storage_size` is too small, the clients will collect inputs
	/// intermittently, but on the other side, a too large value may introduce
	/// virtual delay.
	///
	/// With 60 iteration per seconds a good value is `300`, but is adviced to
	/// perform some tests until you find a better suitable value for your needs.
	int player_input_storage_size = 300;

	/// Amount of time an inputs is re-sent to each peer.
	/// Resenging inputs is necessary because the packets may be lost since as
	/// they are sent in an unreliable way.
	int max_redundant_inputs = 50;

	/// Time in seconds between each `tick_speedup` that the server sends to the
	/// client.
	real_t tick_speedup_notification_delay = 0.33;

	/// Used to set the amount of traced frames to determine the connection healt trend.
	///
	/// This parameter depends a lot on the physics iteration per second, and
	/// an optimal parameter, with 60 physics iteration per second, is 1200;
	/// that is equivalent of the latest 20 seconds frames.
	///
	/// A smaller value will make the recovery mechanism too noisy and so useless,
	/// on the other hand a too big value will make the recovery mechanism too
	/// slow.
	int network_traced_frames = 1200;

	/// Max tolerance for missing snapshots in the `network_traced_frames`.
	int missing_input_max_tolerance = 4;

	/// Used to control the `player` tick acceleration, so to produce more
	/// inputs.
	real_t tick_acceleration = 2.0;

	/// The "optimal input size" is dynamically updated and its size
	/// change at a rate that can be controlled by this parameter.
	real_t optimal_size_acceleration = 2.5;

	/// The server is several frames behind the client, the maxim amount
	/// of these frames is defined by the value of this parameter.
	///
	/// To prevent introducing virtual lag.
	int server_input_storage_size = 30;

	ControllerType controller_type = CONTROLLER_TYPE_NULL;
	Controller *controller = nullptr;
	DataBuffer inputs_buffer;

	SceneSynchronizer *scene_synchronizer = nullptr;

	LocalVector<int> active_doll_peers;
	// Disabled peers is used to stop information propagation to a particular peer.
	LocalVector<int> disabled_doll_peers;

	bool packet_missing = false;
	bool has_player_new_input = false;

public:
	static void _bind_methods();

public:
	NetworkedController();

	void set_player_input_storage_size(int p_size);
	int get_player_input_storage_size() const;

	void set_max_redundant_inputs(int p_max);
	int get_max_redundant_inputs() const;

	void set_tick_speedup_notification_delay(real_t p_delay);
	real_t get_tick_speedup_notification_delay() const;

	void set_network_traced_frames(int p_size);
	int get_network_traced_frames() const;

	void set_missing_snapshots_max_tolerance(int p_tolerance);
	int get_missing_snapshots_max_tolerance() const;

	void set_tick_acceleration(real_t p_acceleration);
	real_t get_tick_acceleration() const;

	void set_optimal_size_acceleration(real_t p_acceleration);
	real_t get_optimal_size_acceleration() const;

	void set_server_input_storage_size(int p_size);
	int get_server_input_storage_size() const;

	uint64_t get_current_input_id() const;

	const DataBuffer &get_inputs_buffer() const {
		return inputs_buffer;
	}

	DataBuffer &get_inputs_buffer_mut() {
		return inputs_buffer;
	}

	void mark_epoch_as_important();

	void set_doll_peer_active(int p_peer_id, bool p_active);
	const LocalVector<int> &get_active_doll_peers() const;

	void _on_peer_connection_change(int p_peer_id);
	void update_active_doll_peers();

	bool process_instant(int p_i, real_t p_delta);

	/// Returns the server controller or nullptr if this is not a server.
	class ServerController *get_server_controller() const;
	/// Returns the player controller or nullptr if this is not a player.
	class PlayerController *get_player_controller() const;
	/// Returns the doll controller or nullptr if this is not a doll.
	class DollController *get_doll_controller() const;
	/// Returns the no net controller or nullptr if this is not a no net.
	class NoNetController *get_nonet_controller() const;

	bool is_server_controller() const;
	bool is_player_controller() const;
	bool is_doll_controller() const;
	bool is_nonet_controller() const;

public:
	void set_inputs_buffer(const BitArray &p_new_buffer);

	void set_scene_synchronizer(SceneSynchronizer *p_synchronizer);
	SceneSynchronizer *get_scene_synchronizer() const;
	bool has_scene_synchronizer() const;

	/* On server rpc functions. */
	void _rpc_server_send_inputs(Vector<uint8_t> p_data);

	/* On client rpc functions. */
	void _rpc_send_tick_additional_speed(Vector<uint8_t> p_data);

	/* On puppet rpc functions. */
	void _rpc_doll_notify_connection_status(bool p_open);
	void _rpc_doll_send_epoch(uint64_t p_epoch, Vector<uint8_t> p_data);

	void process(real_t p_delta);

	void player_set_has_new_input(bool p_has);
	bool player_has_new_input() const;

private:
	virtual void _notification(int p_what);
};

struct FrameSnapshotSkinny {
	uint64_t id;
	BitArray inputs_buffer;
};

struct FrameSnapshot {
	uint64_t id;
	BitArray inputs_buffer;
	uint64_t similarity;
};

struct Controller {
	NetworkedController *node;

	Controller(NetworkedController *p_node) :
			node(p_node) {}

	virtual ~Controller() {}

	virtual void ready() {}
	virtual uint64_t get_current_input_id() const = 0;
};

struct ServerController : public Controller {
	uint64_t current_input_buffer_id = UINT64_MAX;
	uint32_t ghost_input_count = 0;
	real_t optimal_snapshots_size = 0.0;
	real_t client_tick_additional_speed = 0.0;
	real_t additional_speed_notif_timer = 0.0;
	NetworkTracer network_tracer;
	std::deque<FrameSnapshotSkinny> snapshots;

	/// Used to sync the dolls.
	DataBuffer epoch_state_data;
	uint64_t epoch = 0;
	bool is_epoch_important = false;

	ServerController(
			NetworkedController *p_node,
			int p_traced_frames);

	void process(real_t p_delta);
	uint64_t last_known_input() const;
	virtual uint64_t get_current_input_id() const override;

	void receive_inputs(Vector<uint8_t> p_data);
	int get_inputs_count() const;

	/// Fetch the next inputs, returns true if the input is new.
	bool fetch_next_input();

	void doll_sync(real_t p_delta);

	/// This function updates the `tick_additional_speed` so that the `frames_inputs`
	/// size is enough to reduce the missing packets to 0.
	///
	/// When the internet connection is bad, the packets need more time to arrive.
	/// To heal this problem, the server tells the client to speed up a little bit
	/// so it send the inputs a bit earlier than the usual.
	///
	/// If the `frames_inputs` size is too big the input lag between the client and
	/// the server is artificial and no more dependent on the internet. For this
	/// reason the server tells the client to slowdown so to keep the `frames_inputs`
	/// size moderate to the needs.
	void calculates_player_tick_rate(real_t p_delta);
	void adjust_player_tick_rate(real_t p_delta);
};

struct PlayerController : public Controller {
	uint64_t current_input_id;
	uint64_t input_buffers_counter;
	real_t time_bank;
	real_t tick_additional_speed;

	std::deque<FrameSnapshot> frames_snapshot;
	std::vector<uint8_t> cached_packet_data;

	PlayerController(NetworkedController *p_node);

	void process(real_t p_delta);
	int calculates_sub_ticks(real_t p_delta, real_t p_iteration_per_seconds);
	int notify_input_checked(uint64_t p_input_id);
	uint64_t last_known_input() const;
	uint64_t get_stored_input_id(int p_i) const;
	virtual uint64_t get_current_input_id() const override;

	bool process_instant(int p_i, real_t p_delta);
	real_t get_pretended_delta(real_t p_iteration_per_second) const;

	void store_input_buffer(uint64_t p_id);

	/// Sends an unreliable packet to the server, containing a packed array of
	/// frame snapshots.
	void send_frame_input_buffer_to_server();

	bool can_accept_new_inputs() const;
};

/// The doll controller is kind of special controller, it's using a
/// `ServerController` + `MastertController`.
/// The `DollController` receives inputs from the client as the server does,
/// and fetch them exactly like the server.
/// After the execution of the inputs, the puppet start to act like the player,
/// because it wait the player status from the server to correct its motion.
///
/// There are some extra features available that allow the doll to stay in sync
/// with the server execution (see `soft_reset_to_server_state`) and the possibility
/// for the server to stop the data streaming.
struct DollController : public Controller {
	Interpolator interpolator;
	uint64_t current_epoch = UINT64_MAX;
	real_t advancing_epoch = 0.0;

	NetworkTracer network_tracer;

	DollController(NetworkedController *p_node);
	~DollController();

	virtual void ready() override;
	void process(real_t p_delta);
	// TODO consider make this non virtual
	virtual uint64_t get_current_input_id() const override;

	void receive_epoch(uint64_t p_epoch, Vector<uint8_t> p_data);

	uint64_t next_epoch(real_t p_delta);
};

/// This controller is used when the game instance is not a peer of any kind.
/// This controller keeps the workflow as usual so it's possible to use the
/// `NetworkedController` even without network.
struct NoNetController : public Controller {
	uint64_t frame_id;

	NoNetController(NetworkedController *p_node);

	void process(real_t p_delta);
	virtual uint64_t get_current_input_id() const override;
};

#endif

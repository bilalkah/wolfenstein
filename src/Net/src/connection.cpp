#include "Net/connection.h"
#include <algorithm>
#include <atomic>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#else
#include <ixwebsocket/IXWebSocket.h>
#endif

namespace karakale::net {

bool Inbox::Put(std::span<const std::uint8_t> message, double arrived) {
	const std::scoped_lock lock(mutex_);
	if (message.size() > kMaxMessage || count_ == kSlots) {
		return false;
	}
	const std::size_t slot = (head_ + count_) % kSlots;
	std::ranges::copy(message, slots_[slot].begin());
	sizes_[slot] = message.size();
	arrived_[slot] = arrived;
	++count_;
	return true;
}

std::optional<Received> Inbox::Take(std::span<std::uint8_t> out) {
	const std::scoped_lock lock(mutex_);
	if (count_ == 0) {
		return std::nullopt;
	}
	const std::size_t size = sizes_[head_];
	const std::size_t slot = head_;
	head_ = (head_ + 1) % kSlots;
	--count_;
	if (size > out.size()) {
		return std::nullopt;  // no room for it where it was asked for
	}
	std::copy_n(slots_[slot].begin(), size, out.begin());
	return Received{.size = size, .arrived = arrived_[slot]};
}

double Connection::Now() {
	return std::chrono::duration<double>(
			   std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

namespace {

#ifdef __EMSCRIPTEN__

// The browser's own WebSocket, through Emscripten: its events come on the
// page's main thread, between frames
class BrowserConnection final : public Connection
{
  public:
	explicit BrowserConnection(EMSCRIPTEN_WEBSOCKET_T socket)
		: socket_(socket) {}
	~BrowserConnection() override {
		emscripten_websocket_close(socket_, 1000, "left");
		emscripten_websocket_delete(socket_);
	}
	BrowserConnection(const BrowserConnection&) = delete;
	BrowserConnection& operator=(const BrowserConnection&) = delete;
	BrowserConnection(BrowserConnection&&) = delete;
	BrowserConnection& operator=(BrowserConnection&&) = delete;

	State GetState() const override { return state_; }
	bool Send(std::span<const std::uint8_t> message) override {
		// Emscripten's signature asks for a mutable pointer; it only reads
		return state_ == State::Open &&
			   emscripten_websocket_send_binary(
				   socket_, const_cast<std::uint8_t*>(message.data()),
				   static_cast<std::uint32_t>(message.size())) ==
				   EMSCRIPTEN_RESULT_SUCCESS;
	}

	void Listen() {
		emscripten_websocket_set_onopen_callback(socket_, this, OnOpen);
		emscripten_websocket_set_onmessage_callback(socket_, this, OnMessage);
		emscripten_websocket_set_onclose_callback(socket_, this, OnClose);
		emscripten_websocket_set_onerror_callback(socket_, this, OnError);
	}

  private:
	static bool OnOpen(int /*type*/, const EmscriptenWebSocketOpenEvent*,
					   void* connection) {
		static_cast<BrowserConnection*>(connection)->state_ = State::Open;
		return true;
	}
	static bool OnMessage(int /*type*/,
						  const EmscriptenWebSocketMessageEvent* event,
						  void* connection) {
		if (!event->isText) {
			static_cast<BrowserConnection*>(connection)
				->inbox_.Put(std::span(event->data, event->numBytes), Now());
		}
		return true;
	}
	static bool OnClose(int /*type*/, const EmscriptenWebSocketCloseEvent*,
						void* connection) {
		static_cast<BrowserConnection*>(connection)->state_ = State::Closed;
		return true;
	}
	static bool OnError(int /*type*/, const EmscriptenWebSocketErrorEvent*,
						void* connection) {
		static_cast<BrowserConnection*>(connection)->state_ = State::Closed;
		return true;
	}

	EMSCRIPTEN_WEBSOCKET_T socket_;
	State state_ = State::Connecting;
};

#else

// IXWebSocket: its events come on a thread of its own, so the state is
// atomic and the inbox locks
class NativeConnection final : public Connection
{
  public:
	explicit NativeConnection(const std::string& url) {
		socket_.setUrl(url);
		socket_.disableAutomaticReconnection();
		socket_.setOnMessageCallback(
			[this](const ix::WebSocketMessagePtr& message) {
				switch (message->type) {
					case ix::WebSocketMessageType::Open:
						state_ = State::Open;
						break;
					case ix::WebSocketMessageType::Message:
						if (message->binary) {
							inbox_.Put(
								std::span(reinterpret_cast<const std::uint8_t*>(
											  message->str.data()),
										  message->str.size()),
								Now());
						}
						break;
					case ix::WebSocketMessageType::Close:
					case ix::WebSocketMessageType::Error:
						state_ = State::Closed;
						break;
					default:
						break;
				}
			});
		socket_.start();
	}
	~NativeConnection() override { socket_.stop(); }
	NativeConnection(const NativeConnection&) = delete;
	NativeConnection& operator=(const NativeConnection&) = delete;
	NativeConnection(NativeConnection&&) = delete;
	NativeConnection& operator=(NativeConnection&&) = delete;

	State GetState() const override { return state_.load(); }
	bool Send(std::span<const std::uint8_t> message) override {
		return state_.load() == State::Open &&
			   socket_
				   .sendBinary(ix::IXWebSocketSendData(
					   reinterpret_cast<const char*>(message.data()),
					   message.size()))
				   .success;
	}

  private:
	ix::WebSocket socket_;
	std::atomic<State> state_{State::Connecting};
};

#endif

}  // namespace

std::unique_ptr<Connection> Connection::Open(const std::string& url) {
#ifdef __EMSCRIPTEN__
	EmscriptenWebSocketCreateAttributes attributes;
	emscripten_websocket_init_create_attributes(&attributes);
	attributes.url = url.c_str();
	attributes.protocols = nullptr;
	const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attributes);
	if (socket <= 0) {
		return nullptr;
	}
	auto connection = std::make_unique<BrowserConnection>(socket);
	connection->Listen();
	return connection;
#else
	return std::make_unique<NativeConnection>(url);
#endif
}

}  // namespace karakale::net

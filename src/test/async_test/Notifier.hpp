#include <map>
#include <memory>
#include <userver/engine/mutex.hpp>
#include <vector>

class Session {
  public:
    void push(std::string message);

  private:
};

class Notifier {
  public:
    std::shared_ptr<Session> get_session(int id);
    void push_session(Session &&session);

  private:
    std::map<int, Session> sessions;
    userver::engine::Mutex mutex_;
};

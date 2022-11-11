template<typename Stream, typename... Args>
class StreamDispenser {
 private:
  std::tuple<Args...> args;

 public:
  StreamDispenser(Args... args): args(std::make_tuple(args...)) {}

  std::unique_ptr<Stream> get() const {
    return std::apply([](const Args... args){
        return std::make_unique<Stream>(args...);
      }, args);
  }
};

template<typename Stream, typename... Args>
inline auto make_stream_dispenser(Args... args) {
  return StreamDispenser<Stream, Args...>(args...);
}


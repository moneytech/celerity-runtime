#ifndef CELERITY_RUNTIME
#define CELERITY_RUNTIME

#include <functional>
#include <unordered_map>

namespace cl {
namespace sycl {

struct nd_item {
  size_t get_global() { return 0; }
};

namespace access {

enum class mode { read, write };
}
}  // namespace sycl
}  // namespace cl

namespace celerity {

class buffer;

class range {};
using range_mapper = std::function<void(range, range)>;

class kernel_functor {
 public:
  kernel_functor(range_mapper, std::function<void(cl::sycl::nd_item)>){};
};

template <cl::sycl::access::mode Mode>
class accessor {
 public:
  accessor(buffer& buf) : buf(buf) {}

  void operator=(float value) const {};
  float& operator[](size_t i) const { return somefloat; }

  buffer& get_buffer() { return buf; }

 private:
  mutable float somefloat = 13.37f;
  buffer& buf;
};

// We have to wrap the SYCL handler to support our count/kernel syntax
// (as opposed to work group size/kernel)
class handler {
 public:
  // TODO naming: execution_handle vs handler?
  template <typename name = class unnamed_task>
  void parallel_for(size_t count, kernel_functor) {
    // TODO: Handle access ranges
  }

  template <cl::sycl::access::mode Mode>
  void require(accessor<Mode> a);

 private:
  friend class distr_queue;
  distr_queue& queue;

  static inline size_t instance_count = 0;
  size_t id;

  handler(distr_queue& q) : id(instance_count++), queue(q) {}
};

// Presumably we will have to wrap SYCL buffers as well (as opposed to the code
// samples given in the proposal):
// - We have to assign buffers a unique ID to identify them accross nodes
// - We have to return custom accessors to support the CELERITY range specifiers
class buffer {
 public:
  explicit buffer(size_t size) : size(size), id(instance_count++){};

  template <cl::sycl::access::mode Mode>
  accessor<Mode> get_access(celerity::handler handler) {
    auto a = accessor<Mode>(*this);
    handler.require(a);
    return a;
  };

  size_t get_id() { return id; }

  float operator[](size_t idx) { return 1.f; }

 private:
  static inline size_t instance_count = 0;
  size_t id;
  size_t size;
};

class branch_handle {
 public:
  template <size_t idx>
  void get(buffer){};
};

class distr_queue {
 public:
  void submit(std::function<void(handler& cgh)> cgf);

  // experimental
  // TODO: Can we derive 2nd lambdas args from requested values in 1st?
  void branch(std::function<void(branch_handle& bh)>,
              std::function<void(float)>){};

 private:
  friend handler;
  std::unordered_map<size_t, size_t> buffer_last_writer;

  void add_requirement(size_t task_id, size_t buffer_id,
                       cl::sycl::access::mode mode);
};

}  // namespace celerity

#endif

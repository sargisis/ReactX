#include "reactx/root.hpp"

#include "reconciler.hpp"

namespace rx {

Root::Root(Renderer& renderer) : impl_(std::make_unique<detail::Reconciler>(renderer)) {}
Root::~Root() = default;

void Root::render(Element element) { impl_->render(std::move(element)); }
void Root::flush() { impl_->flush(); }
bool Root::dispatch(NodeHandle node, const std::string& event, const EventData& data) {
    return impl_->dispatch(node, event, data);
}

}  // namespace rx

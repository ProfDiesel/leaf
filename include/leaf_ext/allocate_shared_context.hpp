#pragma once

#include <boost/leaf/context.hpp>

#include <memory>
#include <type_traits>

namespace leaf_ext {

template <class...  H>
using context_storage_t = std::aligned_union_t<0, boost::leaf::leaf_detail::polymorphic_context_impl<boost::leaf::context_type_from_handlers<H...>>>;

template <class Alloc, class...  H>
inline boost::leaf::context_ptr allocate_shared_context( const Alloc &alloc ) noexcept
{
    return std::allocate_shared<Alloc, boost::leaf::leaf_detail::polymorphic_context_impl<boost::leaf::context_type_from_handlers<H...>>>(alloc);
}

template <class Alloc, class...  H>
inline boost::leaf::context_ptr allocate_shared_context( const Alloc &alloc, H && ... ) noexcept
{
    return std::allocate_shared<boost::leaf::leaf_detail::polymorphic_context_impl<boost::leaf::context_type_from_handlers<H...>>>(alloc);
}

}

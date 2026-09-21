// @file FastIndexingContract.h
// @brief Source and installed consumers exercise the same unchecked data API contract.
#pragma once
#include <wse/stew.h>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace wse_test
{
inline void requireIndex(const bool condition_in, const char* const message_in)
{
    if (!condition_in) { throw std::runtime_error(message_in); }
}

template<class Data, class Index, class = void> struct HasIndex : std::false_type {};
template<class Data, class Index>
struct HasIndex<Data, Index, std::void_t<decltype(std::declval<Data&>()[std::declval<Index>()])>> : std::true_type {};

template<class Data, class Value>
void checkRows(Data* p_data_inout)
{
    Data& data_inout = *p_data_inout;
    static_assert(std::is_same_v<decltype(std::declval<Data&>()[0]), Value*>);
    static_assert(std::is_same_v<decltype(std::declval<const Data&>()[0]), const Value*>);
    static_assert(noexcept(std::declval<Data&>()[0]) && noexcept(std::declval<const Data&>()[0]));
    static_assert(HasIndex<Data, int>::value && HasIndex<Data, std::size_t>::value);
    static_assert(!HasIndex<Data, double>::value);
    const Data& view = data_inout;
    for (std::size_t y = 0; y < data_inout.height(); ++y)
    {
        requireIndex(data_inout[y] == data_inout.elements() + y * data_inout.width(), "mutable row aliases storage");
        requireIndex(view[y] == view.elements() + y * view.width(), "const row aliases storage");
        for (std::size_t x = 0; x < data_inout.width(); ++x)
        {
            requireIndex(&data_inout[y][x] == &data_inout.at(x, y), "unchecked and checked elements alias");
        }
    }
}

inline void verifyFastIndexing()
{
    wse::Map<int> map(5, 3);
    checkRows<wse::Map<int>, int>(&map);
    map[2][4] = 27;
    const auto& map_view = map;
    requireIndex(map_view[2U][4] == 27 && map.elements()[14] == 27, "map write persists in original memory");

    // Matrix constructors take rows, columns; Map constructors take width, height.
    wse::Matrix_<float> matrix(3, 5);
    requireIndex(matrix.width() == 5 && matrix.height() == 3, "matrix test shape");
    checkRows<wse::Matrix_<float>, float>(&matrix);
    matrix[2][4] = 4.5F;
    requireIndex(matrix.at(4, 2) == 4.5F, "matrix public mutable indexing");

    using Pixel = wse::Pixel_<std::uint16_t, 4, 16>;
    Pixel pixel;
    static_assert(std::is_same_v<decltype(pixel[0]), std::uint16_t&>);
    static_assert(std::is_same_v<decltype(std::declval<const Pixel&>()[0]), const std::uint16_t&>);
    static_assert(noexcept(pixel[0]) && noexcept(std::declval<const Pixel&>()[0]));
    static_assert(!HasIndex<Pixel, double>::value);
    const Pixel& pixel_view = pixel;
    for (std::size_t channel = 0; channel < 4; ++channel)
    {
        pixel[channel] = static_cast<std::uint16_t>(1000 + channel);
        requireIndex(&pixel[channel] == &pixel.data[channel] && &pixel_view[channel] == &pixel.data[channel],
                     "pixel channel references alias storage without copying");
        requireIndex(pixel_view[channel] == 1000 + channel, "const pixel reads mutable write");
    }

    wse::img3c8_t image(7, 3);
    using ImagePixel = std::remove_pointer_t<decltype(image[0])>;
    checkRows<wse::img3c8_t, ImagePixel>(&image);
    image[2][6][2] = 251;
    const auto& image_view = image;
    requireIndex(image_view[2][6][2] == 251 && image.elements()[20].data[2] == 251,
                 "image row/pixel/channel access preserves original storage");

    wse::Homography homography(3, 3);
    using HomographyValue = std::remove_pointer_t<decltype(homography[0])>;
    checkRows<wse::Homography, HomographyValue>(&homography);
    homography[2][2] = 1.0;
    requireIndex(homography.at(2, 2) == 1.0, "homography public mutable indexing");

    using Mesh = wse::Mesh_<wse::float64_xy, wse::float64_xy>;
    using Vertex = wse::TiePoint_<wse::float64_xy, wse::float64_xy>;
    Mesh mesh(3, 5);
    const Mesh& mesh_view = mesh;
    static_assert(std::is_same_v<decltype(mesh[0]), Vertex*>);
    static_assert(std::is_same_v<decltype(mesh_view[0]), const Vertex*>);
    static_assert(noexcept(mesh[0]) && noexcept(mesh_view[0]));
    static_assert(!HasIndex<Mesh, double>::value);
    mesh[2][4].src.x = 13.0;
    mesh[2][4].dst.y = 17.0;
    requireIndex(mesh_view[2][4].src.x == 13.0 && mesh_view[2][4].dst.y == 17.0,
                 "mesh exposes original vertex references");
    requireIndex(mesh[2] == mesh[0] + 2 * mesh.vertex_width() && mesh_view[2] == mesh[2],
                 "mesh row stride and const view");
}
} // namespace wse_test

// @file engine/wse/test/characterization/opencv_adapter_contract.cpp
// @brief The OpenCV adapter converts every supported type by value and rejects mismatches.
// @details The adapter is the single sanctioned OpenCV surface of the public API, and it only
//          compiles where a consumer supplies OpenCV, so this contract is registered behind
//          WSE_ENABLE_OPENCV_ADAPTER_TESTS. It pins the value round trips of the small data
//          types (size, points, ranges, rect, scalar), the cv::Range order normalization, the
//          cv::Mat round trip over the public interleaved contract, the empty-Mat identity, and
//          the format-mismatch rejection that keeps a wrongly typed Mat from becoming plausible
//          image data.

#include <cv/OpenCvAdapter.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace
{
    int failures = 0;

    void expect(const bool condition_in, const char* const message_in)
    {
        if (!condition_in)
        {
            std::cerr << "FAILED: " << message_in << '\n';
            ++failures;
        }
    }
}

int main()
{
    // Small value types survive the round trip unchanged.
    {
        const wse::Size_<int> size = wse::ocv::fromCvSize(cv::Size_<int>(640, 480));
        expect(size.width() == 640 && size.height() == 480, "Size converts from cv");
        const cv::Size_<int> back = wse::ocv::toCvSize(size);
        expect(back.width == 640 && back.height == 480, "Size converts to cv");

        const wse::Point2_<double> p2 = wse::ocv::fromCvPoint(cv::Point_<double>(1.5, -2.5));
        expect(p2.x == 1.5 && p2.y == -2.5, "Point2 converts from cv");
        const cv::Point_<double> p2back = wse::ocv::toCvPoint(p2);
        expect(p2back.x == 1.5 && p2back.y == -2.5, "Point2 converts to cv");

        const wse::Point3_<float> p3 = wse::ocv::fromCvPoint(cv::Point3_<float>(1.0F, 2.0F, 3.0F));
        expect(p3.x == 1.0F && p3.y == 2.0F && p3.z == 3.0F, "Point3 converts from cv");
        const cv::Point3_<float> p3back = wse::ocv::toCvPoint(p3);
        expect(p3back.z == 3.0F, "Point3 converts to cv");
    }

    // A reversed cv::Range still becomes an ordered WSE range.
    {
        const wse::Range1_<int> range = wse::ocv::fromCvRange<int>(cv::Range(9, 3));
        expect(range.minimum() == 3 && range.maximum() == 9, "cv::Range order is normalized");
        const cv::Range back = wse::ocv::toCvRange(range);
        expect(back.start == 3 && back.end == 9, "Range converts to cv");

        const wse::Range2_<int> area = wse::ocv::fromCvRect(cv::Rect_<int>(10, 20, 30, 40));
        expect(area.min_x() == 10 && area.max_x() == 40 &&
                   area.min_y() == 20 && area.max_y() == 60,
               "cv::Rect converts to min/max bounds");
        const cv::Rect_<int> rect = wse::ocv::toCvRect(area);
        expect(rect.x == 10 && rect.y == 20 && rect.width == 30 && rect.height == 40,
               "Range2 converts to cv::Rect");
    }

    // Scalar/pixel conversion copies each channel.
    {
        cv::Scalar_<std::uint8_t> scalar;
        scalar.val[0] = 10U;
        scalar.val[1] = 20U;
        scalar.val[2] = 30U;
        const auto pixel = wse::ocv::fromCvScalar<std::uint8_t, 3U, 8U>(scalar);
        expect(pixel.data[0] == 10U && pixel.data[1] == 20U && pixel.data[2] == 30U,
               "Scalar converts to pixel");
        const cv::Scalar_<double> back = wse::ocv::toCvScalar(pixel);
        expect(back.val[0] == 10.0 && back.val[2] == 30.0, "Pixel converts to scalar");
    }

    // A cv::Mat round-trips through the interleaved contract, including a padded stride.
    {
        cv::Mat mat(cv::Size(3, 2), CV_8UC3);
        for (int row = 0; row < mat.rows; ++row)
        {
            for (int column = 0; column < mat.cols; ++column)
            {
                auto& pixel = mat.at<cv::Vec3b>(row, column);
                pixel[0] = static_cast<std::uint8_t>(row * 10 + column);
                pixel[1] = static_cast<std::uint8_t>(100 + column);
                pixel[2] = static_cast<std::uint8_t>(200 - row);
            }
        }
        const auto image = wse::ocv::fromCvMat<wse::ePixFormat::CH3D8>(mat);
        expect(image.width() == 3U && image.height() == 2U, "Mat converts to image");
        const cv::Mat back = wse::ocv::toCvMat<wse::ePixFormat::CH3D8>(image);
        expect(back.cols == 3 && back.rows == 2 && back.type() == CV_8UC3,
               "Image converts to Mat");
        bool identical = true;
        for (int row = 0; row < mat.rows; ++row)
        {
            for (int column = 0; column < mat.cols; ++column)
            {
                if (back.at<cv::Vec3b>(row, column) != mat.at<cv::Vec3b>(row, column))
                {
                    identical = false;
                }
            }
        }
        expect(identical, "Mat round trip preserves every sample");
    }

    // An empty Mat is an empty image, not an error.
    {
        const auto image = wse::ocv::fromCvMat<wse::ePixFormat::CH3D8>(cv::Mat());
        expect(image.width() == 0U && image.height() == 0U, "Empty Mat converts to empty image");
    }

    // A wrongly typed Mat is rejected instead of becoming plausible data.
    {
        bool rejected = false;
        try
        {
            static_cast<void>(
                wse::ocv::fromCvMat<wse::ePixFormat::CH3D8>(cv::Mat(cv::Size(2, 2), CV_8UC1)));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        expect(rejected, "A format mismatch is rejected");
    }

    return failures == 0 ? 0 : 1;
}

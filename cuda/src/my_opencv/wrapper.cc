#include "wrapper.hh"

namespace my_cv {
    static inline bool isCorrectClass(unsigned char class_) {
        return class_ == 1;
    }
    cv::Mat_<uchar> rebuildImageFromVector(
        const std::vector<uchar> &labels, unsigned width) {
        auto resMat = cv::Mat_<uchar>(labels.size() / width, width);

        for (auto y = 0; y < resMat.rows; y++) {
            for (auto x = 0; x < resMat.cols; ++x) {
                unsigned char class_ = labels[y * resMat.cols + x];
                resMat[y][x] = (isCorrectClass(class_))? 255: 0;
            }
        }

        return resMat;
    }
}

cv::Rect get_position_barcode(cv::Mat image)
{
    GaussianBlur(image, image, Size(3,3), 5);
    threshold(image, image, 150, 255, THRESH_BINARY);
    erode(image, image, getStructuringElement(MORPH_RECT, Size(2,2)), Point(-1,-1), 1);
    dilate(image, image, getStructuringElement(MORPH_RECT, Size(13,5)), Point(-1,-1), 2 );
    std::vector<std::vector<Point> > contours;
    findContours(image, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    auto index_max = 0;

    for (int i = 1; i < contours.size(); i++)
    {
        if (contours[i].size() > contours[index_max].size())
        {
            index_max = i;
        }
    }

    std::vector<Point> contours_poly;
    approxPolyDP( contours[index_max], contours_poly, 3, true );
    Rect boundRect = boundingRect( contours_poly );
    Mat res_image = imread("barcode-00-01.jpg");
    boundRect.x *= 16;
    boundRect.y *= 16;
    boundRect.width *= 16;
    boundRect.height *= 16;

    return boundRect;
}

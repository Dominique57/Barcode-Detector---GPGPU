#include <chrono>
#include <lbp/lbpCpu.hh>
#include <lbp/lbpGpu.hh>
#include <iostream>
#include <knn/knnGpu.hh>
#include <knn/knnCpu.hh>
#include "entryPoint.hh"

#include "my_opencv/wrapper.hh"

void executeAlgorithm(const std::string& databasePath, const std::string &path) {
    cv::Mat_<uchar> image = cv::imread(path, cv::IMREAD_GRAYSCALE);

    // lbp on cpu
    auto lbpCpu = LbpCpu(image.cols, image.rows);
    auto kmeansCpu = KnnCpu(databasePath, 16, 256);
    auto labelsCpu = std::vector<uchar>(lbpCpu.numberOfPatches());

    std::cout << "Running CPU (1core|1thread)" << std::endl;
    auto start = std::chrono::system_clock::now();

    lbpCpu.run(image);
    kmeansCpu.transform(lbpCpu.getFeatures(), labelsCpu);

    auto end = std::chrono::system_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto elapsedS = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    std::cout << elapsedMs.count() << "ms " << elapsedS.count() << "seconds" << std::endl;

    // lbp on Gpu
    auto lbpGpu = LbpGpu(image.cols, image.rows);
    auto kmeanGpu = KnnGpu(databasePath, 16, 256);
    // auto labelsGpu = Matrix<unsigned char>(1, lbpGpu.numberOfPatches());
    auto labelsGpu = std::vector<uchar>(lbpCpu.numberOfPatches());

    std::cout << "Running GPU (1050ti)" << std::endl;
    auto start2 = std::chrono::system_clock::now();

    lbpGpu.run(image);
    kmeanGpu.transform(lbpGpu.getCudaFeatures(), labelsGpu);

    auto end2 = std::chrono::system_clock::now();
    auto elapsedMs2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    auto elapsedS2 = std::chrono::duration_cast<std::chrono::seconds>(end2 - start2);
    std::cout << elapsedMs2.count() << "ms " << elapsedS2.count() << "seconds" << std::endl;

    // Check features
    std::cout << "Checking if cpu and gpu histogram matrix are the same" << std::endl;
    auto &cpuFeatures = lbpCpu.getFeatures();
    auto &gpuFeatures = lbpGpu.getFeatures();
    for (auto y = 0; y < cpuFeatures.rows; ++y) {
        for (auto x = 0; x < cpuFeatures.cols; ++x) {
            if (cpuFeatures[y][x] != gpuFeatures[y][x]) {
                std::cerr << "y:" << y << " x:" << x << " => (cpu)" << cpuFeatures[y][x]
                << " <> (gpu)" << gpuFeatures[y][x] << std::endl;
                throw std::logic_error("Program failed: histogram matrix's are different !");
            }
        }
    }

    // Check labels
    std::cout << "Checking if cpu and gpu predicted labels are the same" << std::endl;
    for (auto i = 0U; i < labelsCpu.size(); ++i) {
        if (labelsCpu[i] != labelsGpu[i]) {
            std::cerr << "i:" << i << " => (cpu)" << (int)labelsCpu[i] << " <> (gpu)" << (int)labelsGpu[i] << std::endl;
            throw std::logic_error("Program failed: predicted labels are different!");
        }
    }

    // Show image
    std::cout << "Showing predicted matrix as an image" << std::endl;
    {
        auto mat = cv::Mat(labelsGpu.size() / (image.cols / SLICE_SIZE), image.cols / SLICE_SIZE,
                           CV_8UC3, cv::Scalar(0, 0, 0));
        cv::Vec3b random_lut[16] = {
            {84,  0,   255},
            {255, 0,   23},
            {0,   116, 255},
            {255, 100, 0},
            {184, 0,   255},
            {255, 200, 0},
            {255, 0,   124},
            {0,   15,  255},
            {255, 0,   0},
            {108, 255, 0},
            {0,   255, 192},
            {0,   255, 92},
            {255, 0,   224},
            {7,   255, 0},
            {208, 255, 0},
            {0,   216, 255}
        };
        for (auto y = 0; y < mat.rows; y++) {
            for (auto x = 0; x < mat.cols; ++x) {
                cv::Vec3b color = random_lut[labelsGpu[y * mat.cols + x]];
                mat.at<cv::Vec3b>(y, x) = cv::Vec3b(color[2], color[1], color[0]);
            }
        }
        cv::imshow("test", mat * 16);
        cv::waitKey(0);
    }

    // Show specific class
    {
        auto mat = cv::Mat(labelsGpu.size() / (image.cols / SLICE_SIZE), image.cols / SLICE_SIZE, CV_8U);
        for (auto y = 0; y < mat.rows; y++) {
            for (auto x = 0; x < mat.cols; ++x) {
                if (labelsGpu[y * mat.cols + x] == 1 || labelsGpu[y * mat.cols + x] == 12)
                    mat.at<uchar>(y, x) = 255;
                else
                    mat.at<uchar>(y, x) = 0;
            }
        }
        cv::imshow("test", mat);
        cv::waitKey(0);
    }
}

void handleImage(const std::string& databasePath, const std::string &imagePath) {
    auto image_rgb = cv::imread(imagePath);
    cv::Mat_<uchar> image;
    cv::cvtColor(image_rgb, image, cv::COLOR_BGR2GRAY);

    // lbp on Gpu
    auto lbpGpu = LbpGpu(image.cols, image.rows);
    auto kmeanGpu = KnnGpu(databasePath, 16, 256);
    auto labelsGpu = std::vector<uchar>(lbpGpu.numberOfPatches());

    // Run
    lbpGpu.run(image);
    kmeanGpu.transform(lbpGpu.getCudaFeatures(), labelsGpu);

    // Show result
    auto predictedLabels = my_cv::rebuildImageFromVector(labelsGpu, image.cols / SLICE_SIZE);
    auto barcode_rect_ret = get_position_barcode(predictedLabels);
    cv::Rect& barcode_rect = barcode_rect_ret.first;
    cv::Mat res_image = image_rgb.clone();
    cv::rectangle(res_image, barcode_rect, cv::Scalar(255, 0, 255), 5);

    cv::Mat showImage;
    cv::resize(res_image, showImage, cv::Size(800, image.rows * 800 / image.cols));
    cv::imshow("Predicted position of code bar", showImage);
    cv::waitKey(0);
}

void handleVideo(const std::string& databasePath, const std::string &videoPath) {
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened())
        throw std::invalid_argument("Cannot open the video file !");

    // lbp on Gpu
    auto lbpGpu = LbpGpu(
        (unsigned)(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        (unsigned)(cap.get(cv::CAP_PROP_FRAME_HEIGHT))
    );
    auto kmeanGpu = KnnGpu(databasePath, 16, 256);
    auto labelsGpu = std::vector<uchar>(lbpGpu.numberOfPatches());

    cv::Mat frame;
    bool escapePressed = false;
    while (!escapePressed) {
        if (!cap.read(frame)) { // video ended
            cap.set(cv::CAP_PROP_POS_AVI_RATIO, 0);
            continue;
        }

        // Run
        cv::Mat_<uchar> grayImage;
        cv::cvtColor(frame, grayImage, cv::COLOR_BGR2GRAY);
        lbpGpu.run(grayImage);
        kmeanGpu.transform(lbpGpu.getCudaFeatures(), labelsGpu);

        // Show result
        auto predictedLabels = my_cv::rebuildImageFromVector(
            labelsGpu, grayImage.cols / SLICE_SIZE);

        auto barcode_rect_ret = get_position_barcode(predictedLabels);
        cv::Rect& barcode_rect = barcode_rect_ret.first;
        cv::Mat res_image = frame.clone();
        cv::rectangle(res_image, barcode_rect, cv::Scalar(255, 0, 255), 5);

        cv::imshow("Predicted", res_image);
        escapePressed = cv::waitKey(30) == 27;
    }
}

void handleCamera(const std::string& databasePath, unsigned cameraId) {
    cv::VideoCapture cap;
    int apiID = cv::CAP_ANY;      // 0 = autodetect default API
    cap.open(cameraId, apiID);
    if (!cap.isOpened())
        throw std::logic_error("Unable to open default camera !");

    // lbp on Gpu
    auto lbpGpu = LbpGpu(
        (unsigned)(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        (unsigned)(cap.get(cv::CAP_PROP_FRAME_HEIGHT))
    );
    auto kmeanGpu = KnnGpu(databasePath, 16, 256);
    auto labelsGpu = std::vector<uchar>(lbpGpu.numberOfPatches());

    cv::Mat frame;
    for (;;) {
        cap.read(frame);
        if (frame.empty())
            break;

        // Run
        cv::Mat_<uchar> grayImage;
        cv::cvtColor(frame, grayImage, cv::COLOR_BGR2GRAY);
        lbpGpu.run(grayImage);
        kmeanGpu.transform(lbpGpu.getCudaFeatures(), labelsGpu);

        // Show result
        auto predictedLabels = my_cv::rebuildImageFromVector(
            labelsGpu, grayImage.cols / SLICE_SIZE);

        auto barcode_rect_ret = get_position_barcode(predictedLabels);
        cv::Rect& barcode_rect = barcode_rect_ret.first;
        cv::Mat res_image = frame.clone();
        cv::rectangle(res_image, barcode_rect, cv::Scalar(255, 0, 255), 5);

        cv::imshow("Predicted", res_image);
        if (cv::waitKey(1) == 27)
            break;
    }
    // the camera will be deinitialized automatically in VideoCapture destructor
}

void generatePredictedRgb(const std::string& databasePath, const std::string &imagePath) {
    auto image_rgb = cv::imread(imagePath);
    cv::Mat_<uchar> image;
    cv::cvtColor(image_rgb, image, cv::COLOR_BGR2GRAY);

    // lbp on Gpu
    auto lbpGpu = LbpGpu(image.cols, image.rows);
    auto kmeanGpu = KnnGpu(databasePath, 16, 256);
    auto labelsGpu = std::vector<uchar>(lbpGpu.numberOfPatches());

    // Run
    lbpGpu.run(image);
    kmeanGpu.transform(lbpGpu.getCudaFeatures(), labelsGpu);

    // Show result
    auto predictedLabels = my_cv::rebuildImageFromVectorRgb(labelsGpu, image.cols / SLICE_SIZE);
    cv::imshow("Predicted classes", predictedLabels);
    cv::waitKey(0);
}

void generateLbpOutFile(const std::string& databasePath, const std::vector<std::string> &imagePaths) {
    auto indexName = 0;
    for (const auto &imagePath: imagePaths) {
        auto image_rgb = cv::imread(imagePath);
        cv::Mat_<uchar> image;
        cv::cvtColor(image_rgb, image, cv::COLOR_BGR2GRAY);

        // lbp on Gpu
        auto lbpGpu = LbpGpu(image.cols, image.rows);
        auto kmeanGpu = KnnGpu(databasePath, 16, 256);
        auto labelsGpu = std::vector<uchar>(lbpGpu.numberOfPatches());

        // Run
        lbpGpu.run(image);
        auto mat = lbpGpu.getFeatures();

        // Declare what you need
        auto outPath = imagePath + ".txt";
        cv::FileStorage file(outPath, cv::FileStorage::WRITE);
        file << "matName" << mat;
        indexName += 1;
    }
}

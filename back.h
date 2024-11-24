#pragma once

#include "opencv2/opencv.hpp"

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>

void setprogress(const char* inp);

short int percent = -1;
short int startRender = 0;

const std::map<std::string, int> str2backend{
    {"opencv", cv::dnn::DNN_BACKEND_OPENCV}, {"cuda", cv::dnn::DNN_BACKEND_CUDA},
    {"timvx",  cv::dnn::DNN_BACKEND_TIMVX},  {"cann", cv::dnn::DNN_BACKEND_CANN}
};
const std::map<std::string, int> str2target{
    {"cpu", cv::dnn::DNN_TARGET_CPU}, {"cuda", cv::dnn::DNN_TARGET_CUDA},
    {"npu", cv::dnn::DNN_TARGET_NPU}, {"cuda_fp16", cv::dnn::DNN_TARGET_CUDA_FP16}
};

class YuNet
{
public:
    YuNet(const std::string& model_path,
        const cv::Size& input_size = cv::Size(320, 320),
        float conf_threshold = 0.6f,
        float nms_threshold = 0.3f,
        int top_k = 5000,
        int backend_id = 3,
        int target_id = 1)
        : model_path_(model_path), input_size_(input_size),
        conf_threshold_(conf_threshold), nms_threshold_(nms_threshold),
        top_k_(top_k), backend_id_(backend_id), target_id_(target_id)
    {
        model = cv::FaceDetectorYN::create(model_path_, "", input_size_, conf_threshold_, nms_threshold_, top_k_, backend_id_, target_id_);
    }

    /* Overwrite the input size when creating the model. Size format: [Width, Height].
    */
    void setInputSize(const cv::Size& input_size)
    {
        input_size_ = input_size;
        model->setInputSize(input_size_);
    }

    cv::Mat infer(const cv::Mat image)
    {
        cv::Mat res;
        model->detect(image, res);
        return res;
    }

private:
    cv::Ptr<cv::FaceDetectorYN> model;

    std::string model_path_;
    cv::Size input_size_;
    float conf_threshold_;
    float nms_threshold_;
    int top_k_;
    int backend_id_;
    int target_id_;
};

YuNet* model;

std::vector<std::wstring> split(std::wstring s, std::wstring delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::wstring token;
    std::vector<std::wstring> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::wstring::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

int renderVid(std::wstring input, std::wstring output)
{
    if (!model)
    {
        model = new YuNet("face_detection_yunet_2023mar.onnx");
    }

    system("rd /s /q tmp");

    system("md tmp");
    system("md tmp\\in");
    system("md tmp\\out");

    std::wstring string = L"ffprobe -v 0 -of csv=p=0 -select_streams v:0 -show_entries stream=r_frame_rate \"";
    string += input;
    string += L"\" > framerate";

    _wsystem(string.c_str());

    std::wifstream file("framerate");

    file >> string;

    file.close();

    std::vector<std::wstring> strfv = split(string, L"/");

    unsigned int framerate = 0;

    framerate = std::stoi(strfv[0]) / std::stoi(strfv[1]);

    system("del framerate");//-vf select='between(t,2,6)+between(t,15,24)'

    string = L"ffmpeg -i \"";
    string += input;
    string += L"\" tmp/in/%d.png";

    _wsystem(string.c_str());//-vf select='between(t,2,6)+between(t,15,24)'

    auto it = std::filesystem::directory_iterator{ "tmp/in" };
    int size = std::count_if(it, {}, [](auto& x) {return x.is_regular_file(); });

    percent = 0;

    for (unsigned int k = 0; k < size - 1; k++)
    {
        cv::String imgname = "tmp/in/";
        imgname += std::to_string(k + 1);
        imgname += ".png";

        cv::Mat image = cv::imread(imgname);

        // Inference
        model->setInputSize(image.size());
        auto faces = model->infer(image);

        // Print faces
        for (int i = 0; i < faces.rows; ++i)
        {
            float conf = faces.at<float>(i, 14);

            if (conf > 0.65)
            {
                int x1 = static_cast<int>(faces.at<float>(i, 0));
                int y1 = static_cast<int>(faces.at<float>(i, 1));
                int w = static_cast<int>(faces.at<float>(i, 2));
                int h = static_cast<int>(faces.at<float>(i, 3));
                std::cout << cv::format("%d: x1=%d, y1=%d, w=%d, h=%d, conf=%.4f\n", k, x1, y1, w, h, conf);

                cv::Rect roi;
                roi.x = x1 + 2;
                roi.y = y1 + 2;
                roi.width = w - 3;
                roi.height = h - 3;

                if (roi.x >= 0 && roi.y >= 0 && roi.width + roi.x < image.cols && roi.height + roi.y < image.rows)
                {
                    cv::Mat crop = image(roi);

                    cv::blur(crop, crop, cv::Size(200, 2));

                    image.copyTo(crop);
                }

                //cv::rectangle(image, cv::Rect(static_cast<int>(faces.at<float>(i, 0)) - 10, static_cast<int>(faces.at<float>(i, 1)) - 50, static_cast<int>(faces.at<float>(i, 2)) + 30, static_cast<int>(faces.at<float>(i, 3)) + 60), cv::Scalar{ 20,120,200 }, -1);
            }
        }

        imgname = "tmp/out/";
        imgname += std::to_string(k + 1);
        imgname += ".png";

        cv::imwrite(imgname, image);

        percent = int(float(k)/float(size-1)*100.0f);
        //cv::imshow("sdfsdf", image);
        //cv::waitKey(0);
    }

    percent = 100;

    string = L"ffmpeg -y -framerate ";
    string += std::to_wstring(framerate);
    string += L" -i tmp/out/%d.png \"";
    string += output;
    string += L"\"";

    _wsystem(string.c_str());

    system("rd /s /q tmp");

    percent = 101;
    startRender = 0;

    return 0;
}
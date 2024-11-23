#include "opencv2/opencv.hpp"

#include "UI.h"

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>

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

cv::Mat visualize(const cv::Mat& image, const cv::Mat& faces, float fps = -1.f)
{
    static cv::Scalar box_color{ 0, 255, 0 };
    static std::vector<cv::Scalar> landmark_color{
        cv::Scalar(255,   0,   0), // right eye
        cv::Scalar(0,   0, 255), // left eye
        cv::Scalar(0, 255,   0), // nose tip
        cv::Scalar(255,   0, 255), // right mouth corner
        cv::Scalar(0, 255, 255)  // left mouth corner
    };
    static cv::Scalar text_color{ 0, 255, 0 };

    auto output_image = image.clone();

    if (fps >= 0)
    {
        cv::putText(output_image, cv::format("FPS: %.2f", fps), cv::Point(0, 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 2);
    }

    for (int i = 0; i < faces.rows; ++i)
    {
        // Draw bounding boxes
        int x1 = static_cast<int>(faces.at<float>(i, 0));
        int y1 = static_cast<int>(faces.at<float>(i, 1));
        int w = static_cast<int>(faces.at<float>(i, 2));
        int h = static_cast<int>(faces.at<float>(i, 3));
        //cv::rectangle(output_image, cv::Rect(x1, y1, w, h), box_color, 2);

        // Confidence as text
        float conf = faces.at<float>(i, 14);
        cv::putText(output_image, cv::format("%.4f", conf), cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_DUPLEX, 0.5, text_color);

        // Draw landmarks
        for (int j = 0; j < landmark_color.size(); ++j)
        {
            int x = static_cast<int>(faces.at<float>(i, 2 * j + 4)), y = static_cast<int>(faces.at<float>(i, 2 * j + 5));
            cv::circle(output_image, cv::Point(x, y), 2, landmark_color[j], 2);
        }
    }
    return output_image;
}

std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

int main(int argc, char** argv)
{
    YuNet model("face_detection_yunet_2023mar.onnx");

    system("md tmp");
    system("md tmp\\in");
    system("md tmp\\out");

    system("ffprobe -v 0 -of csv=p=0 -select_streams v:0 -show_entries stream=r_frame_rate in.mp4 > framerate");

    std::ifstream file ("framerate");
    std::string frameratestr;

    file >> frameratestr;

    file.close();

    std::vector<std::string> strfv = split(frameratestr, "/");

    unsigned int framerate = 0;

    framerate = std::stoi(strfv[0]) / std::stoi(strfv[1]);

    frameratestr = "ffmpeg -framerate ";
    frameratestr += std::to_string(framerate);
    frameratestr += " -i tmp/out/%d.png output.mp4";

    system("ffmpeg -i in.mp4 tmp/in/%d.png");//-vf select='between(t,2,6)+between(t,15,24)'

    auto it = std::filesystem::directory_iterator{ "tmp/in" };
    int size = std::count_if(it, {}, [](auto& x) {return x.is_regular_file(); });

    for (unsigned int k = 0; k < size-1; k++)
    {
        cv::String imgname = "tmp/in/";
        imgname += std::to_string(k+1);
        imgname += ".png";

        cv::Mat image = cv::imread(imgname);

        // Inference
        model.setInputSize(image.size());
        auto faces = model.infer(image);

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
                roi.x = x1+2;
                roi.y = y1+2;
                roi.width = w-3;
                roi.height = h-3;

                if (roi.x >= 0 && roi.y >= 0 && roi.width + roi.x < image.cols && roi.height + roi.y < image.rows)
                {
                    cv::Mat crop = image(roi);

                    GaussianBlur(crop, crop, cv::Size(35, 35), 500, 500);

                    image.copyTo(crop);
                }

                //cv::rectangle(image, cv::Rect(static_cast<int>(faces.at<float>(i, 0)) - 10, static_cast<int>(faces.at<float>(i, 1)) - 50, static_cast<int>(faces.at<float>(i, 2)) + 30, static_cast<int>(faces.at<float>(i, 3)) + 60), cv::Scalar{ 20,120,200 }, -1);
            }
        }

        imgname = "tmp/out/";
        imgname += std::to_string(k + 1);
        imgname += ".png";

        cv::imwrite(imgname, image);
        //cv::imshow("sdfsdf", image);
        //cv::waitKey(0);
    }

    system(frameratestr.c_str());

    system("rd /s /q tmp");

    //main_ui();

    return 0;
}
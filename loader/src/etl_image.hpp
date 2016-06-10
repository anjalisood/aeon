#pragma once
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "etl_interface.hpp"
#include "params.hpp"


namespace nervana {
    namespace image {
        class config;
        class params;
        class decoded;

        class param_factory; // goes from config -> params

        class extractor;
        class transformer;
        class loader;
    }

    class image::params : public nervana::params {
    public:

        params() {}
        void dump(std::ostream & = std::cout);

        cv::Rect cropbox;
        cv::Size2i output_size;
        int angle = 0;
        bool flip = false;
        std::vector<float> lighting;  //pixelwise random values
        float color_noise_std = 0;
        std::vector<float> photometric;  // contrast, brightness, saturation
    };

    class image::param_factory {
    public:
        param_factory(std::shared_ptr<image::config>);
        ~param_factory() {}

        std::shared_ptr<image::params> make_params(std::shared_ptr<const decoded>,
                                                           std::default_random_engine&);
    private:
        void scale_cropbox(const cv::Size2f&, cv::Rect&, float, float);
        void shift_cropbox(const cv::Size2f&, cv::Rect&, float, float);

        bool _do_area_scale;
        std::shared_ptr<image::config> _icp;
    };

    class image::config : public json_config_parser {
    public:
        int height;
        int width;

        std::uniform_real_distribution<float> scale{1.0f, 1.0f};
        std::uniform_int_distribution<int>    angle{0, 0};
        std::normal_distribution<float>       lighting{0.0f, 0.0f};
        std::uniform_real_distribution<float> aspect_ratio{1.0f, 1.0f};
        std::uniform_real_distribution<float> photometric{0.0f, 0.0f};
        std::uniform_real_distribution<float> crop_offset{0.5f, 0.5f};
        std::bernoulli_distribution           flip{0};

        bool do_area_scale = false;
        bool channel_major = true;
        int channels = 3;

        config(std::string argString)
        {
            auto js = nlohmann::json::parse(argString);

            parse_req(height, "height", js);
            parse_req(width, "width", js);

            parse_opt(do_area_scale, "do_area_scale", js);
            parse_opt(channels, "channels", js);
            parse_opt(channel_major, "channel_major", js);

            auto dist_params = js["distribution"];
            parse_dist(angle, "angle", dist_params);
            parse_dist(scale, "scale", dist_params);
            parse_dist(lighting, "lighting", dist_params);
            parse_dist(aspect_ratio, "aspect_ratio", dist_params);
            parse_dist(photometric, "photometric", dist_params);
            parse_dist(crop_offset, "crop_offset", dist_params);
            parse_dist(flip, "flip", dist_params);
            validate();
        }

    private:
        bool validate() {
            return crop_offset.param().a() <= crop_offset.param().b();
        }
    };


    class image::decoded : public decoded_media {
    public:
        decoded() {}
        decoded(cv::Mat img) : _img(img) { _images.push_back(_img); }
        decoded(const std::vector<cv::Mat>& images) { _images = images; }
        virtual ~decoded() override {}

        virtual MediaType get_type() override { return MediaType::IMAGE; }
        cv::Mat& get_image(int index) { return _images[index]; }
        cv::Size2i get_image_size() const {return _images[0].size(); }
        size_t size() const { return _images.size(); }

    private:
        cv::Mat _img;
        std::vector<cv::Mat> _images;
    };


    class image::extractor : public interface::extractor<image::decoded> {
    public:
        extractor(std::shared_ptr<const image::config>);
        ~extractor() {}
        virtual std::shared_ptr<image::decoded> extract(char*, int) override;

        const int get_channel_count() {return _color_mode == CV_LOAD_IMAGE_COLOR ? 3 : 1;}
    private:
        int _pixel_type;
        int _color_mode;
    };


    class image::transformer : public interface::transformer<image::decoded, image::params> {
    public:
        transformer(std::shared_ptr<const image::config>);
        ~transformer() {}
        virtual std::shared_ptr<image::decoded> transform(
                                                std::shared_ptr<image::params>,
                                                std::shared_ptr<image::decoded>) override;

    private:
        void rotate(const cv::Mat& input, cv::Mat& output, int angle);
        void resize(const cv::Mat& input, cv::Mat& output, const cv::Size2i& size);
        void lighting(cv::Mat& inout, std::vector<float>, float color_noise_std);
        void cbsjitter(cv::Mat& inout, const std::vector<float>&);

        // These are the eigenvectors of the pixelwise covariance matrix
        const float _CPCA[3][3];
        const cv::Mat CPCA;

        // These are the square roots of the eigenvalues of the pixelwise covariance matrix
        const cv::Mat CSTD;

        // This is the set of coefficients for converting BGR to grayscale
        const cv::Mat GSCL;
    };


    class image::loader : public interface::loader<image::decoded> {
    public:
        loader(std::shared_ptr<const image::config>);
        ~loader() {}
        virtual void load(char*, int, std::shared_ptr<image::decoded>) override;

    private:
        void split(cv::Mat&, char*);
        bool _channel_major;
    };
}

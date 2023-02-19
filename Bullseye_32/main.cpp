#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>
#include <zbar.h>
#include <opencv2/opencv.hpp>
#include <stdlib.h>

using namespace std;

#include <iostream>
#include <thread>
#include <alsa/asoundlib.h>

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

void play_wav(const char* file_name) {
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file '%s'\n", file_name);
        return;
    }

    // Get file size
    fseek(file, 0L, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    // Read file header
    char header[44];
    if (fread(header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Failed to read file header\n");
        fclose(file);
        return;
    }

    // Parse header
    int sample_rate = *(int*)(header + 24);    
    int num_channels = 2;
    int bits_per_sample = 16;

    // Calculate duration
    int header_size = 44;  // assume WAV format
    double duration = (double)(filesize - header_size) / (sample_rate * num_channels * bits_per_sample / 8);
    // Play sound file
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, num_channels);
    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_hw_params(handle, params);
    snd_pcm_uframes_t frames = 32;
    snd_pcm_prepare(handle);
    unsigned char buffer[4 * frames];
    int bytes_per_frame = num_channels * bits_per_sample / 8;
    long num_frames = (long)(duration * sample_rate);
    for (long i = 0; i < num_frames / frames; i++) {
        for (int j = 0; j < frames; j++) {
            if (fread(buffer + j * bytes_per_frame, bytes_per_frame, 1, file) != 1) {
                fprintf(stderr, "Failed to read from file\n");
                fclose(file);
                snd_pcm_close(handle);
                return;
            }
        }
        snd_pcm_writei(handle, buffer, frames);
    }
    long remaining_frames = num_frames % frames;
    for (int j = 0; j < remaining_frames; j++) {
        if (fread(buffer + j * bytes_per_frame, bytes_per_frame, 1, file) != 1) {
            fprintf(stderr, "Failed to read from file\n");
            fclose(file);
            snd_pcm_close(handle);
            return;
        }
    }
    snd_pcm_writei(handle, buffer, remaining_frames);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    fclose(file);
}


// int main() {
//     const char* file_name = "example.wav";
//     std::thread t(play_wav, file_name);
//     t.join();
//     return 0;
// }


// Create zbar scanner
zbar::ImageScanner scanner;
std::vector<std::string> written_data;
size_t fileNameIndex = 0;
int framesSinceLastDetection = 0;

struct decodedObject
{
    string type;
    string data;
    vector <cv::Point> location;
};

void playSound(){
    // system("cvlc --play-and-exit /home/pi/QR_scanner_Raspberry_Pi/Bullseye_32/build/beep.wav");
     const char* file_name = "/home/pi/QR_scanner_Raspberry_Pi/Bullseye_32/build/beep.wav";
     //play_wav(file_name);
     std::thread t(play_wav, file_name);
     t.join();
}


// Display barcode and QR code location
void display(cv::Mat &im, vector<decodedObject>&decodedObjects)
{
    // Loop over all decoded objects
    for(size_t i = 0; i<decodedObjects.size(); i++){
        vector<cv::Point> points = decodedObjects[i].location;
        vector<cv::Point> hull;

        // If the points do not form a quad, find convex hull
        if(points.size() > 4) cv::convexHull(points, hull);
        else                  hull = points;

        // Number of points in the convex hull
        size_t n = hull.size();

        for(size_t j=0; j<n; j++){
            cv::line(im, hull[j], hull[ (j+1) % n], cv::Scalar(255,0,0), 3);
        }
    }
}

void decode(cv::Mat &im, vector<decodedObject>&decodedObjects, int nb_frames)
{
    if (framesSinceLastDetection == 0 || framesSinceLastDetection > 5)
    {
        // Convert image to grayscale
        cv::Mat imGray;

        cv::cvtColor(im, imGray, cv::COLOR_BGR2GRAY);

        // Wrap image data in a zbar image
        zbar::Image image(im.cols, im.rows, "Y800", (uchar*)imGray.data, im.cols*im.rows);

        // Scan the image for barcodes and QRCodes
        int res = scanner.scan(image);

        if (res > 0 && (framesSinceLastDetection == 0 || framesSinceLastDetection > 5) ) {
            framesSinceLastDetection= 1;
            // Print results
            for(zbar::Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol){
                decodedObject obj;

                obj.type = symbol->get_type_name();
                obj.data = symbol->get_data();
                // Obtain location

                for(int i = 0; i< symbol->get_location_size(); i++){
                    obj.location.push_back(cv::Point(symbol->get_location_x(i),symbol->get_location_y(i)));
                }
                decodedObjects.push_back(obj);

                // debug - print type and data
                // cout << nb_frames << endl;
                // cout << "Type : " << obj.type << endl;
                // cout << "Data : " << obj.data << endl << endl; 
                // decodedObjects[i].data.c_str()
            }
            
            for(size_t i = 0; i<decodedObjects.size(); i++){
                std::string data = decodedObjects[i].data;
                std::stringstream ss;
                ss << "/home/pi/QR_scanner_Raspberry_Pi/Bullseye_32/build/output_" << fileNameIndex << ".bin";
                // std::ofstream output_file(ss.str(), std::ios::binary);
                // if(!output_file.is_open()) {
                //     std::cout << "Error: Could not open file " << ss.str() << std::endl;
                //     return;
                // }
                if (std::find(written_data.begin(), written_data.end(), data) == written_data.end()) {
                    std::ofstream output_file(ss.str(), std::ios::binary);
                    output_file.write(data.c_str(), data.size());
                    written_data.push_back(data);
                    output_file.close();
                    fileNameIndex++;
                    playSound();
                } else {
                    std::cout << "Data already written to a file." << std::endl;
                    //playSound();
                }
            }
            display(im, decodedObjects);
        }
        else
        {
            framesSinceLastDetection++;
        }
    }
    else
    {
        framesSinceLastDetection++;
    }
}
/// For the Raspberry Pi 64-bit Bullseye OS

std::string gstreamer_pipeline(int capture_width, int capture_height, int framerate, int display_width, int display_height) {
    return
            " libcamerasrc ! video/x-raw, "
            //" contrast=(int)5,"
            " width=(int)" + std::to_string(capture_width) + ","
            " height=(int)" + std::to_string(capture_height) + ","
            " framerate=(fraction)" + std::to_string(framerate) +"/1 !"
            " videoconvert ! videoscale !"
            " video/x-raw,"
            //" format=(string)GRAY8,"
            //" contrast=(int)5,"
            " width=(int)" + std::to_string(display_width) + ","
            " height=(int)" + std::to_string(display_height) + " ! videobalance contrast=1.7 saturation=0 ! appsink";
}



int main()
{
    int ch=0;
    int nb_frames=0;
    cv::Mat image;
    float f;
    float FPS[16];
    int i, Fcnt=0;

    chrono::steady_clock::time_point Tbegin, Tend;

    for(i=0;i<16;i++) FPS[i]=0.0;

    //pipeline parameters
    //keep this resolution!!!
    //it will be cropped to 720x720
    int capture_width = 1024;
    int capture_height = 768;
    int framerate = 30;
    int display_width = 1024;
    int display_height = 768;

    //reset frame average
    std::string pipeline = gstreamer_pipeline(capture_width, capture_height, framerate,
                                              display_width, display_height);
    std::cout << "Using pipeline: \n\t" << pipeline << "\n\n\n";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if(!cap.isOpened()) {
        std::cout<<"Failed to open camera."<<std::endl;
        return (-1);
    }

    // Configure scanner
    // see: http://zbar.sourceforge.net/api/zbar_8h.html#f7818ad6458f9f40362eecda97acdcb0
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0);
    scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1);
    scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_BINARY, 1);

    std::cout<<"Sample program for scanning QR codes"<<std::endl;
    std::cout<<"Press ESC to stop."<<std::endl;

    while(ch!=27){
    	if (!cap.read(image)) {
            std::cout<<"Capture read error"<<std::endl;
            break;
        }
        //capture
        cv::Mat crop_img = image(cv::Range(24,744), cv::Range(152,872));

        // Find and decode barcodes and QR codes
        vector<decodedObject> decodedObjects;
        decode(image, decodedObjects, nb_frames++);

        //calculate frame rate (just for your convenience)
        Tend = chrono::steady_clock::now();
        f = chrono::duration_cast <chrono::milliseconds> (Tend - Tbegin).count();
        Tbegin = Tend;
        if(f>0.0) FPS[((Fcnt++)&0x0F)]=1000.0/f;
        for(f=0.0, i=0;i<16;i++){ f+=FPS[i]; }
        putText(image, cv::format("FPS %0.2f", f/16),cv::Point(10,20),cv::FONT_HERSHEY_SIMPLEX,0.6, cv::Scalar(0, 0, 255));
        //std::cout << std::to_string(1000.0/f) << std::endl;

        //cv::resize(image, cv::Size(800, 600));
        //show result
        cv::imshow("Video",image);
        ch=cv::waitKey(10);
    }
    cap.release();
    cv::destroyWindow("Video");
    return 0;
}

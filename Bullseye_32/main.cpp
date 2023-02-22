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
#include <pigpio.h>

#define SWITCH_PIN 18

char header[44];
int num_channels;
int sample_rate;
int bits_per_sample;
double duration;
unsigned char* buffer;
int prevState = -2;
cv::Mat imGray;


void play_wav(){
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
    snd_pcm_prepare(handle);
    
    snd_pcm_uframes_t frames = 32;
    int bytes_per_frame = num_channels * bits_per_sample / 8;
    long num_frames = (long)(duration * sample_rate);
    for (long i = 0; i < num_frames / frames; i++) {
        snd_pcm_writei(handle, buffer + i * frames * bytes_per_frame, frames);
    }
    long remaining_frames = num_frames % frames;
    snd_pcm_writei(handle, buffer + (num_frames / frames) * frames * bytes_per_frame, remaining_frames);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
}

void load_wav(const char* file_name) {
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
    
    if (fread(header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Failed to read file header\n");
        fclose(file);
        return;
    }

    // Parse header
    num_channels = header[22];
    sample_rate = *(int*)&header[24];
    bits_per_sample = header[34];

    // Calculate duration
    int header_size = 44;  // assume WAV format
    duration = (double)(filesize - header_size) / (sample_rate * num_channels * bits_per_sample / 8);

    // Load file into buffer
    buffer = (unsigned char*)malloc(filesize - header_size);
    if (fread(buffer, sizeof(char), filesize, file) != filesize - header_size) {
        fprintf(stderr, "Failed to read file data\n");
        free(buffer);
        return;
    }
    fclose(file);    
}

// Create zbar scanner
zbar::ImageScanner scanner;
std::vector<std::string> written_data;
size_t fileNameIndex = 0;
int framesSinceLastDetection = 0;
int discardFramesAfterScan = -1;

struct decodedObject
{
    string type;
    string data;
    vector <cv::Point> location;
};

void playSound(){
     std::thread t(play_wav);
     t.detach();
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
    if (framesSinceLastDetection == 0 || framesSinceLastDetection > discardFramesAfterScan)
    {


        // Wrap image data in a zbar image
        zbar::Image image(im.cols, im.rows, "Y800", (uchar*)im.data, im.cols*im.rows);

        // Scan the image for barcodes and QRCodes
        int res = scanner.scan(image);

        if (res > 0 && (framesSinceLastDetection == 0 || framesSinceLastDetection > discardFramesAfterScan) ) {
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
                    playSound();
                    std::ofstream output_file(ss.str(), std::ios::binary);
                    output_file.write(data.c_str(), data.size());
                    written_data.push_back(data);
                    output_file.close();
                    fileNameIndex++;
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
            //" vflip=(bool)1,"
            //" ev=0,"
            // " shutter=(float)10.0,"
            " framerate=(fraction)" + std::to_string(framerate) +"/1 !"
            " videoconvert !"
            " video/x-raw,"
            //" format=(string)GRAY8,"
            //" contrast=(int)5,"
            " width=(int)" + std::to_string(display_width) + ","
            " height=(int)" + std::to_string(display_height) +
            " ! videobalance contrast=2.0 saturation=0" +
            //" ! videobalance saturation=0" +
            " ! appsink";

}

int main()
{
    if (gpioInitialise() < 0) {
        std::cerr << "Error: Failed to initialize pigpio library" << std::endl;
        return 1;
    }

    // Set SWITCH_PIN as input with pull-up resistor enabled
    gpioSetMode(SWITCH_PIN, PI_INPUT);
    gpioSetPullUpDown(SWITCH_PIN, PI_PUD_UP);

    const char* file_name = "/home/pi/QR_scanner_Raspberry_Pi/Bullseye_32/build/beep.wav";
    load_wav(file_name);
    playSound();

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

    int scannedAlreadyThisTurn = 0;
    int grabCount = 0; // number of low-cpu grabs after the switch is pushed. used this to introduce a delay before detecting

    while(ch!=27){
        int pressed = gpioRead(SWITCH_PIN);
        
        if (prevState == 0 && pressed == 1){
            scannedAlreadyThisTurn = 0;
            framesSinceLastDetection = 0;
            nb_frames = 9;
            std::cout<<grabCount<<std::endl;
            grabCount = 0;
            std::cout<<"scannedAlreadyThisTurn = 0, framesSinceLastDetection = 0"<<std::endl;
        }
        prevState = pressed;
        if (pressed == 1 && scannedAlreadyThisTurn == 0 && grabCount > 2)
        {
            // switch closed

            if (!cap.read(image)) {
                std::cout<<"Capture read error"<<std::endl;
                break;
            }
            //capture
            // cv::Mat crop_img = image(cv::Range(24,744), cv::Range(152,872));
            cv::Mat crop_img = image(cv::Range(100,768), cv::Range(0,1024));

            // Find and decode barcodes and QR codes
            vector<decodedObject> decodedObjects;

            // Convert image to grayscale

            cv::cvtColor(crop_img, imGray, cv::COLOR_BGR2GRAY);

            int pixelValue = (int)imGray.at<uchar>(768-100-20,512);
            cout << pixelValue << endl;
            if (pixelValue < 100){
                decode(imGray, decodedObjects, nb_frames++);

                //calculate frame rate (just for your convenience)
                Tend = chrono::steady_clock::now();
                f = chrono::duration_cast <chrono::milliseconds> (Tend - Tbegin).count();
                Tbegin = Tend;
                if(f>0.0) FPS[((Fcnt++)&0x0F)]=1000.0/f;
                for(f=0.0, i=0;i<16;i++){ f+=FPS[i]; }
                
                //std::cout << std::to_string(1000.0/f) << std::endl;

                //cv::resize(image, cv::Size(800, 600));
                //show result
                if (decodedObjects.size() > 0){
                    scannedAlreadyThisTurn = 1;
                }
                if ((decodedObjects.size() > 0 || nb_frames % 10 == 0) && (framesSinceLastDetection > discardFramesAfterScan || framesSinceLastDetection == 1) )
                {
                    putText(imGray, cv::format("FPS %0.2f", f/16),cv::Point(10,20),cv::FONT_HERSHEY_SIMPLEX,0.6, cv::Scalar(0, 0, 255));
                    cv::imshow("Video",imGray);
                }
            }
        }
        else{
            cap.grab();
            grabCount++;
        }
        ch=cv::waitKey(10);
    }
    cap.release();
    cv::destroyWindow("Video");
    return 0;
}

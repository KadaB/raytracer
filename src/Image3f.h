#ifndef IMAGE3F_H
#define IMAGE3F_H

#include <iostream>

#include <glm/glm.hpp>
#include <cstddef>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <memory>

typedef unsigned char byte;

float clamp(float val, float min = 0.0f, float max = 1.0f) {
	return std::max(std::min(val, max), min);
}

class Image3f {
	const std::string ppmHeader = "P6";
public:

	int width;
	int height;

	glm::vec3 **data;

	Image3f(int width, int height) {
		this->width = width;
		this->height = height;

		// initialize 2D Array
		data = new glm::vec3*[height];
		for (int y = 0; y < height; y++) {
			data[y] = new glm::vec3[width];
		}
	}

	~Image3f() {
		// free mem
		for (int y = 0; y < height; y++) {
			delete[] data[y];
		}
		delete[] data;
	}

	// write pixel to image
	void setAt(int x, int y, glm::vec3 color) {
		data[y][x] = color;
	}

	// read pixel from image
	glm::vec3 getAt(int x, int y) {
		return data[y][x];
	}


	// write ppm from vec3
	void write3fPpm(std::string filename) {
		using namespace std;
		ofstream ofs(filename, ios_base::out | ios_base::binary);
		ofs << ppmHeader << endl << width << ' ' << height << endl << "255"
				<< endl;

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				glm::vec3 color = getAt(x, y);
				byte r = (byte) (clamp(color[0]) * 255);
				byte g = (byte) (clamp(color[1]) * 255);
				byte b = (byte) (clamp(color[2]) * 255);
				ofs << r << g << b;
			}
		}

		ofs.close();
	}

	// create rgb gradient image
	void rgbImage(glm::vec3 nwColor = glm::vec3(0, 0, 1),  // A - top left corner
				  glm::vec3 neColor = glm::vec3(0, 1, 0), // B - top right corner
				  glm::vec3 swColor = glm::vec3(1, 0, 0), // C - bottom right
				  glm::vec3 seColor = glm::vec3(1, 1, 0)) { // D - bottom left

		// interpolate over the result of two interpolation levels: lerp(lerp(A,B,s), lerp(D,C,s), t)
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float s = (float) x / (float) (width - 1);// s left right percentage
				float t = (float) y / (float) (height - 1);	// t top down percentage

				glm::vec3 nwe = (1 - s) * nwColor + s * neColor; // lerp top left vs right
				glm::vec3 swo = (1 - s) * swColor + s * seColor; // lerp bottom left vs right
				glm::vec3 color = (1 - t) * nwe + t * swo;	// lerp top vs bottom
				setAt(x, y, color);
			}
		}
	}

	// Draw test checker pattern
	void checker(int patternSize,
				 glm::vec3 colorA = glm::vec3(1, 0, 0),
				 glm::vec3 colorB = glm::vec3(0, 1, 0)) {

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float patternOnOff = ((x / patternSize + (y / patternSize % 2))
						% 2);
				glm::vec3 color = patternOnOff * colorA
						+ (1.0f - patternOnOff) * colorB;
				setAt(x, y, color);
			}
		}
	}

//	// p6!
//	// https://www.scratchapixel.com/lessons/digital-imaging/simple-image-manipulations
//	bool loadPpm(string filename) {
//		std::ifstream ifs;
//
//		ifs.open(filename, std::ios::binary);
//
//		if (ifs.fail) {
//			cout << "could not read file: " << filename << endl;
//			return false;
//		}
//
//		std::string header;
//		ifs >> header;
//		if (header.compare(ppmHeader) != 0) {
//			cout << "PPM header not recognized or not supported version: " << header << endl;
//			return false;
//		}
//
//		int imageWidth, imageHeight, colVals;
//		ifs >> imageWidth >> imageHeight >> colVals;
//
//	}

	int display() {
		std::unique_ptr<float> buffer(get_3f_bgr_buffer());
		cv::Mat flt_img(height, width, CV_32FC3, buffer.get()); /* Red Green Blue Alpha (RGBA) channels from the sdl surface */
		cv::imshow("Display ", flt_img);
		return cv::waitKey(0);
	}

	void save(std::string filename) {
		std::unique_ptr<byte> buffer(get_3b_bgr_buffer());
		cv::Mat flt_img(height, width, CV_8UC3, buffer.get()); /* Red Green Blue Alpha (RGBA) channels from the sdl surface */
		cv::imwrite(filename, flt_img);
		std::cout << "Written to: " << filename << std::endl;
	}

private:
	float* get_3f_bgr_buffer() {
		float *buffer = new float[width * height * 3];

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				glm::vec3 color = getAt(x, y);
				int offset = (y * width + x) * 3;

				buffer[offset] = color[2];
				buffer[offset + 1] = color[1];
				buffer[offset + 2] = color[0];
			}
		}

		return buffer;
	}

	byte* get_3b_bgr_buffer() {
		byte *buffer = new byte[width * height * 3];

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				glm::vec3 color = getAt(x, y);
				int offset = (y * width + x) * 3;

				buffer[offset]  = (byte) (clamp(color[2]) * 255);
				buffer[offset+1]= (byte) (clamp(color[1]) * 255);
				buffer[offset+2]= (byte) (clamp(color[0]) * 255);
			}
		}

		return buffer;
	}
};

#endif

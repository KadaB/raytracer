#ifndef IMAGE3F_H
#define IMAGE3F_H

#include <iostream>

#include <glm/glm.hpp>
#include <cstddef>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <memory>

inline float clamp(float val, float min = 0.0f, float max = 1.0f) {
	return glm::max(glm::min(val, max), min);
}

inline glm::vec3 clampRGB(glm::vec3 color) {
	return glm::vec3(clamp(color[0]), clamp(color[1]), clamp(color[2]));
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
				unsigned char r = (unsigned char) (clamp(color[0]) * 255);
				unsigned char g = (unsigned char) (clamp(color[1]) * 255);
				unsigned char b = (unsigned char) (clamp(color[2]) * 255);
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
	// examples:
    //	  image.checker(16, glm::vec3(0.6f, 0.6f, 0.6f), glm::vec3(0.8f, 0.8f, 0.8f));
    //    image.checker(16, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0));
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

	int display(int ms) {
		auto buffer = get_3f_bgr_buffer();
		cv::Mat flt_img(height, width, CV_32FC3, buffer.get()); /* Red Green Blue Alpha (RGBA) channels from the sdl surface */
		cv::imshow("Display ", flt_img);
		return cv::waitKey(ms);
	}

	void save(std::string filename) {
		auto buffer = get_3b_bgr_buffer();
		cv::Mat flt_img(height, width, CV_8UC3, buffer.get()); /* Red Green Blue Alpha (RGBA) channels from the sdl surface */
		cv::imwrite(filename, flt_img);
		std::cout << "Written to: " << filename << std::endl;
	}

private:
	std::unique_ptr<float[]> get_3f_bgr_buffer() {
		auto buffer_ptr = std::make_unique<float[]>(width * height * 3);
		float *buffer = buffer_ptr.get();

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				glm::vec3 color = getAt(x, y);
				int offset = (y * width + x) * 3;

				buffer[offset] = color[2];
				buffer[offset + 1] = color[1];
				buffer[offset + 2] = color[0];
			}
		}

		return buffer_ptr;
	}

	std::unique_ptr<unsigned char[]> get_3b_bgr_buffer() {
		auto buffer_ptr = std::make_unique<unsigned char[]>(width * height * 3);
		unsigned char *buffer = buffer_ptr.get();

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				glm::vec3 color = getAt(x, y);
				int offset = (y * width + x) * 3;

				buffer[offset]  = (unsigned char) (clamp(color[2]) * 255);
				buffer[offset+1]= (unsigned char) (clamp(color[1]) * 255);
				buffer[offset+2]= (unsigned char) (clamp(color[0]) * 255);
			}
		}

		return buffer_ptr;
	}
};

#endif

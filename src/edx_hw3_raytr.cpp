//============================================================================
// Name        : edx_hw3_raytr.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <limits>

#include "Image3f.h"

#include <glm/glm.hpp>
#include <cstddef>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "readScene.h"

using namespace std;
using namespace glm;

bool intersectSphere(glm::vec3 O, glm::vec3 OP, glm::vec3 S, float r, float &out_t) {
	const float t = glm::dot(S - O, OP);
	glm::vec3 Q = O + t * OP;

	const float y = glm::length(S - Q);
	const float discriminant = r*r - y*y;

	if(discriminant > 0) { 		// two intersections
		const float x = glm::sqrt(discriminant);

		const float t1 = t + x;
		const float t2 = t - x;

		if(t1 <= t2) {
			out_t = t1;
		}
		else {
			out_t = t2;
		}

		return true;
	}
	else if(discriminant < 0) {
		return false;
	}
	else { // exactly 0 one intersection
		out_t = t;
		return true;
	}
}

// ABC = points of triangle, n = triangle normal, p = intersection point p with plane
bool isInTriangle(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 n, glm::vec3 p) {
	glm::vec3 nAB = glm::cross(B-A, n);
	glm::vec3 nBC = glm::cross(C-B, n);
	glm::vec3 nCA = glm::cross(A-C, n);

	const float pDotAB = glm::dot(nAB, p - A);
	const float pDotBC = glm::dot(nBC, p - B);
	const float pDotCA = glm::dot(nCA, p - C);

	return pDotAB <= 0 && pDotBC <= 0 && pDotCA <= 0;
}

bool isInTriangleBC(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 p) {
	glm::vec3 nABp = glm::cross(B - A, p - A);
	glm::vec3 nBCp = glm::cross(C - B, p - B);
	glm::vec3 nCAp = glm::cross(A - C, p - C);
	glm::vec3 nABC = glm::cross(B - A, C - A);

	const float areaABC = glm::length(nABC);
	const float areaABp = glm::length(nABp);
	const float areaBCp = glm::length(nBCp);
	const float areaCAp = glm::length(nCAp);

	const float v = areaBCp / areaABC;
	const float u = areaCAp / areaABC;
	const float w = areaABp / areaABC;

	const float sum = v + u + w;
	return sum >= 0 && sum <= 1.0;
}

// origin = camera origin/eye, rayDir = direction/line of ray, triangles = list of triangles
// return true on intersect, false else
bool intersectTriangle(glm::vec3 origin, glm::vec3 rayDir, glm::vec3 A, glm::vec3 B, glm::vec3 C, float &out_t) {
		// plane equation
		glm::vec3 n = glm::normalize(glm::cross(B-A, C-A));
		const float t = ( glm::dot(A, n) - glm::dot(origin, n) ) / glm::dot(n, rayDir);

		// point of plane intersection
		glm::vec3 p = origin + t * rayDir;

		// point in triangle test
		if(isInTriangle(A, B, C, n, p)) {
			out_t = t;
			return true;
		}

		return false;
}

int intersectAllGeometry(glm::vec3 origin, glm::vec3 rayDir, SceneReader &sceneReader) {
	float min_t = std::numeric_limits<float>::max();
	int hitObject = -1;

	for(int i = 0; i < sceneReader.geometry.size(); i++) {
		IndexedGeometry &geometry = sceneReader.geometry[i];
		float cur_t;

		bool intersection = false;
		if(geometry.geometryType == GeometryType::SPHERE) {
			intersection = intersectSphere(origin, rayDir, geometry.position, geometry.radius, cur_t);
		}
		else if(geometry.geometryType == GeometryType::TRIANGLE) {
			glm::vec3 A = sceneReader.vertices[geometry.vertexIndices[0]];
			glm::vec3 B = sceneReader.vertices[geometry.vertexIndices[1]];
			glm::vec3 C = sceneReader.vertices[geometry.vertexIndices[2]];

			intersection = intersectTriangle(origin, rayDir, A, B, C, cur_t);
		}

		if(intersection) {
			if(cur_t < min_t) {
				min_t = cur_t;
				hitObject = i;
			}
		}
	}

	return hitObject;
}

int main() {
	cout << "Hello Brother. Writing image." << endl;
	SceneReader sr;
	sr.readScene("res/scene3.test");
	sr.camera.updateAxes();

	const int width = sr.camera.width;
	const int height = sr.camera.height; // image dims

	Image3f image(width, height);

	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			glm::vec3 rayDir = sr.camera.getRayAt((float) x + .5, (float) y + .5);

			const int hitIndex = intersectAllGeometry(sr.camera.eye, rayDir, sr);
			if(hitIndex >= 0) {
				IndexedGeometry &geometry = sr.geometry[hitIndex];

				glm::vec3 color = sr.ambientColors[geometry.ambientIndex];
				image.setAt(x, y, color);
			}
		}
	}

	image.display();

	return 0;
}

void test() {
//	image.checker(16);
//	string filename = "rgb.ppm";
//	image.write3fPpm(filename);
//	cout << "Done write image: " << filename << endl;
}


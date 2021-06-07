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
#include <omp.h>

#include "Image3f.h"

#include <glm/glm.hpp>
#include <cstddef>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <chrono>

#include "readScene.h"

using namespace std;
using namespace glm;

struct HitInfo {
	bool validHit;
	float t; // t hit for ray (world space)
	int hitIndex; // index of hit
	glm::vec3 position; // exact/fragment position of hit
	glm::vec3 fragmentNormal; // normal for fragment hit

	glm::vec3 colorTransport;

	HitInfo() {
		this->validHit = false;
		this->hitIndex = -1;
		this->position = glm::vec3();
		this->fragmentNormal = glm::vec3();
		this->t = 0;
	}

	HitInfo(bool validHit, float t, glm::vec3 position, glm::vec3 fragmentNormal) : HitInfo() {
		this->validHit = validHit;
		this->position = position;
		this->fragmentNormal = fragmentNormal;
		this->t = t;
	}
};

HitInfo intersectSphere(glm::vec3 O, glm::vec3 OP, glm::vec3 S, float r) {
	const float t = glm::dot(S - O, OP);
	glm::vec3 Q = O + t * OP;

	const float y = glm::length(S - Q);
	const float discriminant = r*r - y*y;

	if(discriminant > 0) { 		// two intersections
		const float x = glm::sqrt(discriminant);

		const float t1 = t + x;
		const float t2 = t - x;

		if(t1 > 0 && t2 >0 ) {
		if(t1 <= t2) {
			return HitInfo(true, t1, O + t1 * OP, (O + t1 * OP) - S);
		}
		else {
			return HitInfo(true, t2, O + t2 * OP, (O + t2 * OP) - S);
		}
		}
	}
	else if(discriminant < 0) {
		return HitInfo(false, t, glm::vec3(), glm::vec3());
	}
	else { // exactly 0 one intersection
		return HitInfo(t>0, t,  Q, Q - S);
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
HitInfo intersectTriangle(glm::vec3 origin, glm::vec3 rayDir, glm::vec3 A, glm::vec3 B, glm::vec3 C) {
		// plane equation
		glm::vec3 n = glm::normalize(glm::cross(B-A, C-A));
		const float t = ( glm::dot(A, n) - glm::dot(origin, n) ) / glm::dot(n, rayDir);

		// point of plane intersection
		glm::vec3 p = origin + t * rayDir;

		// point in triangle test
		if(isInTriangle(A, B, C, n, p) && t > 0) {
			return HitInfo(true, t, p, n); // TODO: flat shading, interpolate normal
		}

		return HitInfo();
}

glm::vec3 normalTransformPoint(glm::mat4 transform, glm::vec3 vector) {
	return glm::vec3(glm::transpose(glm::inverse(transform)) * glm::vec4(vector, 1));
}
glm::vec3 inverseTransformPoint(glm::mat4 transform, glm::vec3 vector) {
	return glm::vec3(glm::inverse(transform) * glm::vec4(vector, 1));
}
glm::vec3 transformPoint(glm::mat4 transform, glm::vec3 vector) {
	return glm::vec3(transform * glm::vec4(vector, 1));
}

HitInfo intersectAllGeometry(glm::vec3 origin, glm::vec3 rayDir, SceneReader &sceneReader) {
	float min_t = std::numeric_limits<float>::max();
	HitInfo rsltHitInfo;

	for(int i = 0; i < sceneReader.geometry.size(); i++) {
		IndexedGeometry &geometry = sceneReader.geometry[i];
		HitInfo cur_hitInfo; // intersection information

		const glm::vec3 new_origin = inverseTransformPoint(geometry.transform, origin);
		const glm::vec3 new_rayPoint = inverseTransformPoint(geometry.transform, origin + rayDir);
		glm::vec3 new_rayDir = new_rayPoint - new_origin;
		const float rayDirLen = glm::length(new_rayDir);
		new_rayDir = glm::normalize(new_rayDir);

		if(geometry.geometryType == GeometryType::SPHERE) {
			glm::vec3 position = geometry.position;
			cur_hitInfo = intersectSphere(new_origin, new_rayDir, position, geometry.radius);
		}
		else if(geometry.geometryType == GeometryType::TRIANGLE) {
			glm::vec3 A = sceneReader.vertices[geometry.vertexIndices[0]];
			glm::vec3 B = sceneReader.vertices[geometry.vertexIndices[1]];
			glm::vec3 C = sceneReader.vertices[geometry.vertexIndices[2]];

			cur_hitInfo = intersectTriangle(new_origin, new_rayDir, A, B, C);
		}

		const float cur_t = cur_hitInfo.t / rayDirLen;
		if(cur_hitInfo.validHit) {
			if(cur_t < min_t) {
				min_t = cur_t;
				rsltHitInfo = cur_hitInfo;
				rsltHitInfo.t = cur_t;
				rsltHitInfo.hitIndex = i;
				rsltHitInfo.position = origin + cur_t * rayDir;
				rsltHitInfo.fragmentNormal =
						normalTransformPoint(geometry.transform, rsltHitInfo.fragmentNormal);
				rsltHitInfo.colorTransport = geometry.diffuseColor;
			}
		}
	}

	return rsltHitInfo;
}

void raytrace(std::string scenefilename) {
	SceneReader sr;
	sr.readScene(scenefilename);
	sr.camera.updateAxes();
//	return 0;

	std::cout<<"initialize image buffer space"<<std::endl;
	const int width = sr.camera.width;
	const int height = sr.camera.height; // image dims

	Image3f image(width, height);
	std::cout<<"setting background"<<std::endl;
	image.checker(16, glm::vec3(0.6f, 0.6f, 0.6f), glm::vec3(0.8f, 0.8f, 0.8f));
//	image.checker(16, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0));

	std::cout<<"start raytrace" << std::endl;

	auto start = std::chrono::high_resolution_clock::now();

	#pragma omp parallel for
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			glm::vec3 rayDir = sr.camera.getRayAt(x, y);

			HitInfo hitInfo = intersectAllGeometry(sr.camera.eye, rayDir, sr);
			if(hitInfo.validHit) {
				IndexedGeometry &geometry = sr.geometry[hitInfo.hitIndex];
				glm::vec3 color = geometry.ambientColor + geometry.emissionColor;

				// shadowray
				for(int i = 0; i < sr.lights.size(); i++) {
					Light &light = sr.lights[i];

					glm::vec3 shadowray_direction;
					if(light.type == LightType::POINT) {
						shadowray_direction = glm::normalize(light.position - hitInfo.position);
					}
					else {
						shadowray_direction = glm::normalize(light.position);
					}

					glm::vec3 shadowray_origin = hitInfo.position + 0.001f*shadowray_direction;

					HitInfo shadowHitInfo = intersectAllGeometry(shadowray_origin, shadowray_direction, sr);
					if(shadowHitInfo.validHit) {
						hitInfo.colorTransport = glm::vec3(0, 0, 0);
					}
					else {
						// lambert shading .. first ...
						// winkel zu Licht.
						const float lambertShade =
								clamp(glm::dot(glm::normalize(shadowray_direction), glm::normalize(hitInfo.fragmentNormal)));

						glm::vec3 lambert = geometry.diffuseColor * light.color * lambertShade;
						// phong
						glm::vec3 halfvec = glm::normalize(glm::normalize(shadowray_direction) + glm::normalize(-rayDir));
//						glm::vec3 halfvec = glm::normalize(shadowray_direction - rayDir);
						const float phongShade =
								clamp(glm::dot(halfvec, glm::normalize(hitInfo.fragmentNormal)));

						const float shinePow = glm::pow(std::max(phongShade, 0.f), geometry.shininess);
						glm::vec3 phong = geometry.specularColor * light.color * shinePow;

						hitInfo.colorTransport = lambert + phong;
					}
				}

				image.setAt(x, y, hitInfo.colorTransport + geometry.ambientColor + geometry.emissionColor);
			}
		}
	}

	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "finished raytracing after " << elapsed.count() << " seconds" << std::endl;

	int k = image.display();
//	std::cout << "last keycode pressed: " << k << std::endl;
	if(k == 10) { // || !sr.outputFilename.empty()) {
		std::string filename =
				sr.outputFilename.empty() ? "raytrace.png" :  sr.outputFilename;
		image.save(filename);
	}
}

int main() {
//	raytrace("res/scene1.test");
//	raytrace("res/scene2.test");
//	raytrace("res/scene3.test");
//	raytrace("res/scene4-ambient.test");
//	raytrace("res/scene4-diffuse.test");
//	raytrace("res/scene4-emission.test");
//	raytrace("res/scene4-specular.test");
	raytrace("res/scene5.test");
//	raytrace("res/scene6.test");
//	raytrace("res/scene7.test");
	return 0;
}

void test() {
//	image.checker(16);
//	string filename = "rgb.ppm";
//	image.write3fPpm(filename);
//	cout << "Done write image: " << filename << endl;
}


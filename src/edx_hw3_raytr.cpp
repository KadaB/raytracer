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
#include <cstddef>
#include <chrono>

#include "Image3f.h"

#include <glm/glm.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "readScene.h"

using namespace std;
using namespace glm;

struct HitInfo {
	bool validHit;
	float t; 				  // t hit for ray (world space and sphere space)
	glm::vec3 fragmentNormal; // normal for fragment hit in sphere(object) space

	HitInfo() {
		this->validHit = false;
		this->fragmentNormal = glm::vec3();
		this->t = 0;
	}

	HitInfo(bool validHit, float t, glm::vec3 fragmentNormal) : HitInfo() {
		this->validHit = t > 0 ? validHit : false;
		this->fragmentNormal = fragmentNormal;
		this->t = t;
	}
};


struct FragmentInfo {
	bool validHit = false;
	glm::vec3 position;		// fragment position in world space
	glm::vec3 normal;		// normal in world space
	int objectIndex;		// object that was hit
};

HitInfo intersectSphereAnalytic(glm::vec3 O, glm::vec3 D, glm::vec3 S, float r) {

	glm::vec3 Q = O - S;
	float p = 2*glm::dot(Q, D) / glm::dot(D, D);
	float q = (glm::dot(Q, Q) - r*r) / glm::dot(D, D);

	float discriminant = p*p / 4 - q; // discriminant is the term under the root you uneducated idiot.

	if(discriminant == 0) {
		float x = -p/2;

		return HitInfo(true, x, (O + x*D) - S);
	}
	else if (discriminant > 0) {
		float root = glm::sqrt(discriminant);

		float x1 = -p/2 + root;
		float x2 = -p/2 - root;

		// nicht besonders performant, aber größere von beiden, wenn einer 0 war der Grund des Unterschieds bei scene4-specular im Schatten
		float t = glm::min(x1, x2);
		if(x1 <= 0) t = x2;
		if(x2 <= 0) t = x1;

        return HitInfo(true, t, (O + t*D) - S);
	}

	return HitInfo();
}

HitInfo intersectSphere(glm::vec3 O, glm::vec3 OP, glm::vec3 S, float r) {
	const float t = glm::dot(S - O, OP) / glm::dot(OP, OP);
	glm::vec3 Q = O + t * OP;

	const float y = glm::length(S - Q);
	const float discriminant = r*r - y*y;

	if(discriminant > 0) { 		// two intersections
		const float x = glm::sqrt(discriminant) / glm::length(OP);

		const float t1 = t + x;
		const float t2 = t - x;

		if(t1 > 0 && t2 >0 ) {
			if(t1 <= t2) {
				return HitInfo(true, t1, (O + t1 * OP) - S);
			}
			else {
				return HitInfo(true, t2, (O + t2 * OP) - S);
			}
		}
	}
	else if(discriminant < 0) {
		return HitInfo(false, t, glm::vec3());
	}
	else { // exactly 0 one intersection
		return HitInfo(t>0, t, Q - S);
	}

	return HitInfo();
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


float detTerm3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c, int index) {
	int index1 = index % 3;
	int index2 = (index + 1) % 3;
	int index3 = (index + 2) % 3;
	return a[index1] * b[index2] * c[index3] - c[index1] * b[index2] * a[index3];
}
// gibt auch glm::determinant
float det3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
	float t1 = detTerm3x3(a, b, c, 0);
	float t2 = detTerm3x3(a, b, c, 1);
	float t3 = detTerm3x3(a, b, c, 2);
	return t1 + t2 + t3;
}
/**
 * a, b, c = column vector 1, 2, 3 in Matrix A
 * d = Solution column vector
 * A = (a, b, c)
 * A * x = d
 * returns column vector x which A multiplied with has result d
 */
glm::vec3 solve3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
	float detA = det3x3(a, b, c);
	float x = det3x3(d, b, c) / detA;
	float y = det3x3(a, d, c) / detA;
	float z = det3x3(a, b, d) / detA;
	return glm::vec3(x, y, z);
}

HitInfo intersectTriangleAnalytic(glm::vec3 origin, glm::vec3 rayDir, glm::vec3 A, glm::vec3 B, glm::vec3 C) {
	glm::vec3 solution = solve3x3(A-B, A-C, rayDir, A-origin);
	if(solution.x >= 0 && solution.y>=0 && solution.x + solution.y <= 1 && solution.z >0) {
		return HitInfo(true, solution.z, glm::normalize(glm::cross(B-A, C-A)));
	}
	return HitInfo();
}


// origin = camera origin/eye, rayDir = direction/line of ray, triangles = list of triangles
// return true on intersect, false else
HitInfo intersectTriangle(glm::vec3 origin, glm::vec3 rayDir, glm::vec3 A, glm::vec3 B, glm::vec3 C) {
		// plane equation
		glm::vec3 n = glm::normalize(glm::cross(B-A, C-A));
		const float t = glm::dot(A - origin, n) / glm::dot(n, rayDir);

		// point of plane intersection
		glm::vec3 p = origin + t * rayDir;

		// point in triangle test
		//if(isInTriangleBC(A, B, C, p) && t > 0) {
		if(isInTriangle(A, B, C, n, p)) { // t < 0 in hit = false
			return HitInfo(true, t, n); // TODO: flat shading, interpolate normal
		}

		return HitInfo();
}

glm::vec3 normalTransform(glm::mat4 transform, glm::vec3 vector) {
	return glm::normalize(glm::vec3(glm::transpose(glm::inverse(transform)) * glm::vec4(vector, 0)));
}
glm::vec3 transformPoint(glm::mat4 transform, glm::vec3 vector) {
	return glm::vec3(transform * glm::vec4(vector, 1));
}
// w=0
glm::vec3 transformDirection(glm::mat4 transform, glm::vec3 vector) {
	return glm::vec3(transform * glm::vec4(vector, 0));
}

FragmentInfo intersectScene(glm::vec3 origin, glm::vec3 rayDir, SceneReader &sceneReader) {
	float min_wrld_t = std::numeric_limits<float>::max();
	FragmentInfo fragmentInfo;

	for(int i = 0; i < sceneReader.geometry.size(); i++) {
		IndexedGeometry &geometry = sceneReader.geometry[i];
		HitInfo cur_hitInfo; // intersection information

		// origin and ray in object/sphere space
		const glm::vec3 new_origin = transformPoint(glm::inverse(geometry.transform), origin);
		glm::vec3 new_rayDir = transformDirection(glm::inverse(geometry.transform), rayDir);

		if(geometry.geometryType == GeometryType::SPHERE) {
			glm::vec3 position = geometry.position;
			cur_hitInfo = intersectSphereAnalytic(new_origin, new_rayDir, position, geometry.radius);
		}
		else if(geometry.geometryType == GeometryType::TRIANGLE) {
			glm::vec3 A = sceneReader.vertices[geometry.vertexIndices[0]];
			glm::vec3 B = sceneReader.vertices[geometry.vertexIndices[1]];
			glm::vec3 C = sceneReader.vertices[geometry.vertexIndices[2]];

			cur_hitInfo = intersectTriangleAnalytic(new_origin, new_rayDir, A, B, C);
		}

		if(cur_hitInfo.validHit) {
            const float t = cur_hitInfo.t;

			if(t < min_wrld_t) {
				min_wrld_t = t;
				fragmentInfo.validHit = true;
				fragmentInfo.objectIndex = i;
				fragmentInfo.position = origin + t * rayDir; // hit in world space coordinate
				fragmentInfo.normal = normalTransform(geometry.transform, cur_hitInfo.fragmentNormal);	 // normal in world space, needs to be transformed
			}
		}
	}

	return fragmentInfo;
}

glm::vec3 shadowRayTest(FragmentInfo fragmentInfo, IndexedGeometry &geometry, glm::vec3 rayDir, SceneReader &sr) {
	glm::vec3 shadowColor(0, 0, 0);

	for(int i = 0; i < sr.lights.size(); i++) {
		Light &light = sr.lights[i];

		bool isInLight = false;
		glm::vec3 shadowray_direction;
		float attenuation = 1.0f;

        shadowray_direction = light.type == LightType::POINT ?
        		  glm::normalize(light.position - fragmentInfo.position)
				: glm::normalize(light.position);

        glm::vec3 shadowray_origin = fragmentInfo.position + sr.epsilonBias * shadowray_direction;
        FragmentInfo shadowHitInfo = intersectScene(shadowray_origin, shadowray_direction, sr);

		if(light.type == LightType::POINT) {
            const float light_distance = glm::length(light.position - fragmentInfo.position);
            const float hit_dist = glm::length(shadowHitInfo.position - fragmentInfo.position);
            const bool inShadow = shadowHitInfo.validHit && hit_dist < light_distance;

            isInLight = !inShadow;

			attenuation = light.attenuation[0]
                        + light.attenuation[1] * light_distance
                        + light.attenuation[2] * light_distance * light_distance;
		}
		else {
			isInLight = !shadowHitInfo.validHit;
		}

        if(isInLight) {
            // lambert shading .. first ...
            // winkel zu Licht.
            const float lambertShade =
                    clamp(glm::dot(glm::normalize(shadowray_direction), fragmentInfo.normal));

            glm::vec3 lambert = geometry.diffuseColor * light.color * lambertShade;
            // phong
            glm::vec3 halfvec = glm::normalize(glm::normalize(shadowray_direction) + glm::normalize(-rayDir));

            const float phongShade =
                    clamp(glm::dot(halfvec, fragmentInfo.normal));

            const float shinePow = glm::pow(std::max(phongShade, 0.f), geometry.shininess);
            glm::vec3 phong = geometry.specularColor * light.color * shinePow;


            shadowColor += (lambert + phong) / attenuation;
        }
	}

	return clampRGB(shadowColor);
}

glm::vec3 trace(glm::vec3 rayOrigin, glm::vec3 rayDir, SceneReader &sr, const float maxDepth = 5) {
	FragmentInfo fragmentInfo = intersectScene(rayOrigin, glm::normalize(rayDir), sr);
	if(fragmentInfo.validHit) {
		IndexedGeometry &geometry = sr.geometry[fragmentInfo.objectIndex];

		glm::vec3 reflectionColor(0, 0, 0);
		if(maxDepth > 0) {
			// calculate reflectionRay, fragment is in world space
			glm::vec3 fragmentNormal = fragmentInfo.normal;
			glm::vec3 viewDir = glm::normalize(-rayDir);

			glm::vec3 reflectedDir = (2 * glm::dot(viewDir, fragmentNormal) *fragmentNormal) - viewDir;

			glm::vec3 reflectedPos = fragmentInfo.position + sr.epsilonBias * reflectedDir;
			reflectionColor = trace(reflectedPos, reflectedDir, sr, maxDepth - 1);
		}

		// shadowray
		glm::vec3 shadowColor = shadowRayTest(fragmentInfo, geometry, rayDir, sr);

		return clampRGB(
				geometry.ambientColor
				+ geometry.emissionColor
				+ shadowColor
				+ geometry.specularColor * reflectionColor
				);
	}
	else {
		return glm::vec3(0, 0, 0);
	}
}

void raytrace(std::string scenefilename) {
	SceneReader sr;
	sr.readScene(scenefilename);
	sr.camera.updateAxes();
//	return 0;

	std::cout<<"initialize image buffer space"<<std::endl;
	const int width = sr.camera.width;
	const int height = sr.camera.height; // image dims
	std::cout<<"Image " << width << " " << height <<std::endl;

	Image3f image(width, height);
	std::cout<<"setting background"<<std::endl;
//	image.checker(16, glm::vec3(0.6f, 0.6f, 0.6f), glm::vec3(0.8f, 0.8f, 0.8f));
	image.checker(16, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0));

	std::cout<<"start raytrace" << std::endl;

	auto start = std::chrono::high_resolution_clock::now();

	#pragma omp parallel for
	//#pragma omp parallel for collapse(2)
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			glm::vec3 rayDir = sr.camera.getRayAt(x, y);

				image.setAt(x, y,
						trace(sr.camera.eye, rayDir, sr, 5)
						);
		}
	}

	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "finished raytracing after " << elapsed.count() << " seconds" << std::endl;

	int k = image.display(0);
//	std::cout << "last keycode pressed: " << k << std::endl;
	if(k == 10 || !sr.outputFilename.empty()) {
		std::string filename =
				sr.outputFilename.empty() ? "raytrace.png" :  sr.outputFilename;
		image.save(filename);
	}
}

int main() {
//	raytrace("res/test.test");
//	raytrace("res/scene1.test");			// Triangle
//	raytrace("res/scene2.test");			// Würfel
//	raytrace("res/scene3.test");			// table

// (Unit3 alt, HW1 neu)
//	raytrace("res/scene4-ambient.test");	// table
//	raytrace("res/scene4-diffuse.test");
//	raytrace("res/scene4-emission.test");
//	raytrace("res/scene4-specular.test");
//	raytrace("res/scene5.test");		// many spheres, 9 sec
	raytrace("res/scene6.test");		// cornell box
//	raytrace("res/scene7.test");		// ~900 sec (15 min)
	return 0;
}

void test() {
//	image.checker(16);
//	string filename = "rgb.ppm";
//	image.write3fPpm(filename);
//	cout << "Done write image: " << filename << endl;
}


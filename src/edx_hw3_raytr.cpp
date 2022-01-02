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

FragmentInfo intersectScene(glm::vec3 rayOrigin, glm::vec3 rayDir, SceneReader &sceneReader) {
	HitInfo min_hitInfo;
	IIntersectable *min_geometry;

	for(auto const& geometry_ptr : sceneReader.geometries) {
		// origin and ray into inverse object space(_os)
		const glm::vec3 rayOrigin_os = transformPoint(glm::inverse(geometry_ptr->transform), rayOrigin);
		glm::vec3 rayDir_os = transformDirection(glm::inverse(geometry_ptr->transform), rayDir);

		HitInfo hitInfo = geometry_ptr->intersect(rayOrigin_os, rayDir_os);
        if(hitInfo.validHit && hitInfo.t < min_hitInfo.t) {
            min_hitInfo = hitInfo;
            min_geometry = geometry_ptr;
		}
	}

	if(min_hitInfo.validHit) {
		FragmentInfo fragmentInfo;
		fragmentInfo.validHit = true;
		fragmentInfo.t = min_hitInfo.t;
		fragmentInfo.position = rayOrigin + min_hitInfo.t * rayDir;
		fragmentInfo.normal = normalTransform(min_geometry->transform, min_hitInfo.normal);
		fragmentInfo.material = min_hitInfo.material;
		return fragmentInfo;
	}
	else {
		return FragmentInfo();
	}
}

inline glm::vec3 calc_lighting(glm::vec3 rayDir, glm::vec3 shadowray_direction, glm::vec3 fragmentNormal,
		Material *material, glm::vec3 lightColor) {
    // lambert shading
    const float lambertShade = clamp(glm::dot(glm::normalize(shadowray_direction), fragmentNormal));

    glm::vec3 lambert = material->diffuseColor * lightColor * lambertShade;
    // phong shading
    glm::vec3 halfvec = glm::normalize(glm::normalize(shadowray_direction) + glm::normalize(-rayDir));

    const float phongShade = clamp(glm::dot(halfvec, fragmentNormal));

    const float shinePow = glm::pow(std::max(phongShade, 0.f), material->shininess);
    glm::vec3 phong = material->specularColor * lightColor * shinePow;

    return lambert + phong;
}

glm::vec3 shadowRayTest(FragmentInfo fragmentInfo, glm::vec3 rayDir, SceneReader &sr) {
	glm::vec3 shadowColor(0, 0, 0);
	Material *material = fragmentInfo.material;

	for(auto const& light : sr.lights) {
        if(light.type == LightType::POINT) {
            glm::vec3 shadowray_direction = glm::normalize(light.position - fragmentInfo.position);
            glm::vec3 shadowray_origin = fragmentInfo.position + sr.epsilonBias * shadowray_direction;
            FragmentInfo shadowHitInfo = intersectScene(shadowray_origin, shadowray_direction, sr);
//            float distToLight = glm::length(light.position - fragmentInfo.position);
//            float distToHit = glm::length(shadowHitInfo.position - fragmentInfo.position);
            // TODO: quality check!
            float distToLight = glm::dot(light.position - fragmentInfo.position, shadowray_direction);
            float distToHit = shadowHitInfo.t;

            if(!shadowHitInfo.validHit || distToLight < distToHit) {
                float attenuation = light.attenuation[0]
                            + light.attenuation[1] * distToLight
                            + light.attenuation[2] * distToLight * distToLight ;

                shadowColor += calc_lighting(rayDir, shadowray_direction, fragmentInfo.normal, material, light.color) / attenuation;
            }
        }
        else if(light.type == LightType::DIRECTIONAL) {
            glm::vec3 shadowray_direction = glm::normalize(light.position);
            glm::vec3 shadowray_origin = fragmentInfo.position + sr.epsilonBias * shadowray_direction;
            FragmentInfo shadowHitInfo = intersectScene(shadowray_origin, shadowray_direction, sr);

            if(!shadowHitInfo.validHit) {
                shadowColor += calc_lighting(rayDir, shadowray_direction, fragmentInfo.normal, material, light.color);
            }
        }
	}

	return clampRGB(shadowColor);
}

glm::vec3 trace(glm::vec3 rayOrigin, glm::vec3 rayDir, SceneReader &sr, const float maxDepth = 5) {
	FragmentInfo fragmentInfo = intersectScene(rayOrigin, glm::normalize(rayDir), sr);
	if(fragmentInfo.validHit) {
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
		glm::vec3 shadowColor = shadowRayTest(fragmentInfo, rayDir, sr);

		return clampRGB( fragmentInfo.material->ambientColor
                       + fragmentInfo.material->emissionColor
                       + shadowColor
                       + fragmentInfo.material->specularColor * reflectionColor);
	}
	else {
		return glm::vec3(0, 0, 0);
	}
}

void raytrace(std::string scenefilename) {
	SceneReader sr;
	sr.readScene(scenefilename);
	sr.camera.updateAxes();

	std::cout<<"initialize image buffer space"<<std::endl;
	const int width = sr.camera.width;
	const int height = sr.camera.height; // image dims
	std::cout<<"Image " << width << " " << height <<std::endl;

	Image3f image(width, height);
	std::cout<<"setting background"<<std::endl;

	std::cout<<"start raytrace" << std::endl;

	auto start = std::chrono::high_resolution_clock::now();

	#pragma omp parallel for
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			glm::vec3 rayDir = sr.camera.getRayAt(x, y);

            image.setAt(x, y, trace(sr.camera.eye, rayDir, sr, 5));
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

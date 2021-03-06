/*
 * geometries.h
 *
 *  Created on: 02.01.2022
 *      Author: farnsworth
 */

#ifndef SRC_GEOMETRIES_H_
#define SRC_GEOMETRIES_H_

#include <iostream>
#include <vector>
#include <string>
#include <stack>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

float FLOAT_MAX = std::numeric_limits<float>::max();
float FLOAT_MIN = std::numeric_limits<float>::min();
inline float only_positive_scalar(float t) {
	return t > 0 ? t : FLOAT_MAX;
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

struct Material {
	glm::vec3 ambientColor;
	glm::vec3 diffuseColor;
	glm::vec3 specularColor;
	glm::vec3 emissionColor;
	float shininess;

	Material() {
        this->shininess = 0;
	};

	Material(glm::vec3 ambientColor, glm::vec3 diffuseColor, glm::vec3 specularColor, glm::vec3 emissionColor, float shininess) {
        this->ambientColor = ambientColor;
        this->diffuseColor = diffuseColor;
        this->specularColor = specularColor;
        this->emissionColor = emissionColor;
        this->shininess = shininess;
	};
};

// intersection info for object space
struct HitInfo {
	bool validHit;
	float t; 		  // t hit for ray (world space and sphere space)
	glm::vec3 normal; // normal for fragment hit in sphere(object) space
	Material *material;

	HitInfo() {
		this->validHit = false;
		this->normal = glm::vec3();
		this->t = FLOAT_MAX;
		material = NULL;
	}

	HitInfo(bool validHit, float t, glm::vec3 normal, Material *material) : HitInfo() {
		this->validHit = t > 0 ? validHit : false;
		this->normal = normal;
		this->t = only_positive_scalar(t);
		this->material = material;
	}
};

// convenience structure -> type safety for intersection info for world space
struct FragmentInfo {
	bool validHit = false;
	float t;				// length on ray the intersection occurred at
	glm::vec3 position;		// fragment position in world space
	glm::vec3 normal;		// normal in world space
	Material *material;

	FragmentInfo() {
        this->validHit = false;
        this->t = FLOAT_MAX;
        this->position = glm::vec3();
        this->normal = glm::vec3();
        this->material = NULL;
	}

	// convenience constructor
	FragmentInfo(bool validHit, float t, glm::vec3 position, glm::vec3 normal, Material *material) : FragmentInfo() {
        this->validHit = t > 0 ? validHit : false;
        this->t = only_positive_scalar(t);
        this->position = position;
        this->normal = normal;
        this->material = material;
	}
};

struct IIntersectable {
	IIntersectable() {}
	virtual ~IIntersectable() {};
	virtual FragmentInfo intersect(glm::vec3 O, glm::vec3 D, float t_limit = FLT_MAX) = 0;
};

struct ITransformedIntersectable {
	ITransformedIntersectable() {}
	virtual ~ITransformedIntersectable() {};
	virtual HitInfo intersect(glm::vec3 O, glm::vec3 D) = 0;
	virtual std::pair<glm::vec3, glm::vec3> getExtends() = 0;

	Material material;
	glm::mat4 transform;

	// get all 8 corners from bounding box definition start, end
	std::array<glm::vec3, 8> eightCornersFromBoundingBox(glm::vec3 start, glm::vec3 end) {
		glm::vec3 vecs[2] = {start, end};
		std::array<glm::vec3, 8> eight_corners;

		// pick components either from start or end in all combinations
		for(int i = 0; i < 8; i++) {
			int a = (i / 1) % 2, b = (i / 2) % 2, c = (i / 4) % 2;
			eight_corners[i] = glm::vec3(vecs[a].x, vecs[b].y, vecs[c].z);
		}

		return eight_corners;
	}

	std::pair<glm::vec3, glm::vec3> boundingBoxOfTransformedBoundingBox(glm::vec3 start, glm::vec3 end) {
		// auto points = {
        //      glm::vec3(start.x, start.y, start.z),
        //      glm::vec3(start.x, start.y, end.z),
        //      glm::vec3(start.x, end.y, start.z),
        //      glm::vec3(end.x, start.y, start.z),
        //      glm::vec3(end.x, end.y, end.z),
        //      glm::vec3(end.x, end.y, start.z),
        //      glm::vec3(end.x, start.y, end.z),
        //      glm::vec3(start.x, end.y, end.z) };

		glm::vec3 min_start = glm::vec3(1, 1, 1) * FLOAT_MAX;
		glm::vec3 max_end = glm::vec3(1, 1, 1) * FLOAT_MIN;
		for(auto point : this->eightCornersFromBoundingBox(start, end)) {
			auto p = transformPoint(this->transform, point);
			min_start = glm::min(min_start, p);
			max_end = glm::max(max_end, p);
		}

		return {min_start, max_end};
	};
};

class Container : public IIntersectable {
	std::vector<ITransformedIntersectable*> geometries;		// cells of vectors
public:
	Container(std::vector<ITransformedIntersectable*> *geometries_ptr) {
		for(auto const& geometry_ptr : *geometries_ptr) {
			this->add(geometry_ptr);
        }
	};
	Container() { };
	~Container() { };

	void add(ITransformedIntersectable* geometry_ptr) {
		this->geometries.push_back(geometry_ptr);
	};

	virtual FragmentInfo intersect(glm::vec3 rayOrigin, glm::vec3 rayDir, float t_limit = FLT_MAX) {
		// all geometries in cell get brute forced
        HitInfo min_hitInfo;
        ITransformedIntersectable *min_geometry_ptr;
        for(auto const& geometry_ptr : this->geometries)  {

            const glm::vec3 rayOrigin_os = transformPoint(glm::inverse(geometry_ptr->transform), rayOrigin);
            glm::vec3 rayDir_os = transformDirection(glm::inverse(geometry_ptr->transform), rayDir);
            HitInfo hitInfo = geometry_ptr->intersect(rayOrigin_os, rayDir_os);

            // has to be intersection at current cell
            if(hitInfo.validHit && hitInfo.t < min_hitInfo.t && hitInfo.t < t_limit) {
            	min_hitInfo = hitInfo;
            	min_geometry_ptr = geometry_ptr;
            }
        }

        if(min_hitInfo.validHit) {
        	// return FragmentInfo(true, min_hitInfo.t,
        	// 		rayOrigin + min_hitInfo.t * rayDir,
			// 		normalTransform(min_geometry_ptf->transform, min_hitInfo.normal),
			// 		min_hitInfo.material);
			FragmentInfo fragmentInfo;
			fragmentInfo.validHit = true;
			fragmentInfo.t = min_hitInfo.t;
			fragmentInfo.position = rayOrigin + min_hitInfo.t * rayDir;
			fragmentInfo.normal = normalTransform(min_geometry_ptr->transform, min_hitInfo.normal);
			fragmentInfo.material = min_hitInfo.material;
			return fragmentInfo;
        }
        return FragmentInfo();
	};
};



class Sphere : public ITransformedIntersectable {
private:
	glm::vec3 center;
	float radius;
public:
	Sphere(const glm::vec3 center, const float radius, Material material, glm::mat4 transform) {
		this->center = center;
		this->radius = radius;
		this->material = material;
		this->transform = transform;
	};

	virtual HitInfo intersect(glm::vec3 O, glm::vec3 D) {
		float r = this->radius;
		glm::vec3 S = this->center;
		glm::vec3 Q = O - S;

        float p = 2*glm::dot(Q, D) / glm::dot(D, D);
        float q = (glm::dot(Q, Q) - r*r) / glm::dot(D, D);

        float discriminant = p*p / 4 - q; // discriminant is the term under the root

        if(discriminant == 0) {
            float t = -p/2;

            return HitInfo(true, t, (O + t*D) - S, &this->material);
        }
        else if (discriminant > 0) {
            float root = glm::sqrt(discriminant);

            float x1 = -p/2 + root;
            float x2 = -p/2 - root;

            // get the smallest (non-zero) t TODO: this convenient, but not really performant?
            float t = glm::min(x1, x2);
            if(x1 <= 0) t = x2;
            if(x2 <= 0) t = x1;

            return HitInfo(true, t, (O + t*D) - S, &this->material);
        }

        return HitInfo();
	};

	virtual std::pair<glm::vec3, glm::vec3> getExtends() {
		glm::vec3 diagonal = glm::vec3(this->radius,this->radius,this->radius);
		glm::vec3 start = this->center - diagonal;
		glm::vec3 end = this->center + diagonal;

		return this->boundingBoxOfTransformedBoundingBox(start, end);
	}
};

class Triangle : public ITransformedIntersectable {
private:
	glm::vec3 A, B, C;	// 3 vertices of a triangle

	// rule of sarrus, gets the terms for the determinant of a 3x3 Matrix a, b, c are column vectors
	inline float detTerm3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c, int index) {
		int index1 = index % 3;
		int index2 = (index + 1) % 3;
		int index3 = (index + 2) % 3;
		return a[index1] * b[index2] * c[index3] - c[index1] * b[index2] * a[index3];
	};

	// return determinant for matrix (there is also glm::determinant)
	inline float det3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
		float t1 = detTerm3x3(a, b, c, 0);
		float t2 = detTerm3x3(a, b, c, 1);
		float t3 = detTerm3x3(a, b, c, 2);
		return t1 + t2 + t3;
	};
	/**
	 * a, b, c = column vector 1, 2, 3 in Matrix A
	 * d = Solution column vector
	 * A = (a, b, c)
	 * A * x = d
	 * returns column vector x which Matrix A multiplied with has result d
	 */
	glm::vec3 solve3x3(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
		float detA = det3x3(a, b, c);
		float x = det3x3(d, b, c) / detA;
		float y = det3x3(a, d, c) / detA;
		float z = det3x3(a, b, d) / detA;
		return glm::vec3(x, y, z);
	};

public:
	Triangle(glm::vec3 A, glm::vec3 B, glm::vec3 C, Material material, glm::mat4 transform)  {
		this->transform = transform;
		this->material = material;
		this->A = A;
		this->B = B;
		this->C = C;
	};

	virtual HitInfo intersect(glm::vec3 origin, glm::vec3 rayDir) {
        glm::vec3 solution = solve3x3(A-B, A-C, rayDir, A-origin);
        if(solution.x >= 0 && solution.y>=0 && solution.x + solution.y <= 1 && solution.z >0) {
            return HitInfo(true, solution.z, glm::normalize(glm::cross(B-A, C-A)), &this->material);
        }
        return HitInfo();
	};

	virtual std::pair<glm::vec3, glm::vec3> getExtends() {
		auto start = glm::min(glm::min(A, B), C);
		auto end = glm::max(glm::max(A, B), C);

		return this->boundingBoxOfTransformedBoundingBox(start, end);
	}
};

struct Camera {
	glm::vec3 eye;
	glm::vec3 center;
	glm::vec3 worldUp;
	float fovDeg;		 // field of view in degrees

	glm::vec3 u, v, w; // camera frame axes

	int width;
	int height;
	// update/calculate camera frame axes
	void updateAxes() {
		glm::vec3 eyeDir = center - eye;
		w = glm::normalize(-eyeDir);
		u = glm::normalize(glm::cross(worldUp, w));
		v = glm::normalize(glm::cross(w, u));
	};

	glm::vec3 getRayAt(int x, int y) {
		float j = (float) x + 0.5;
		float i = (float) y + 0.5;
		const float aspect = (float) width / (float) height;
		const float tany = glm::tan(fovDeg * glm::pi<float>() / 360.f); //conv to rad and half fovy
		const float tanx = tany * aspect;
		const float halfW = (float) width / 2.f;
		const float halfH = (float) height / 2.f;

		const float a = tanx * ((j - halfW) / halfW);
		const float b = tany * ((halfH - i) / halfH);
		return glm::normalize(a * u + b * v - w);
	};
};

enum LightType {
	POINT,
	DIRECTIONAL,
};

struct Light {
	glm::vec3 position;
	glm::vec3 color;
	LightType type;
	glm::vec3 attenuation;	//c0, c1, c2
};

#endif /* SRC_GEOMETRIES_H_ */

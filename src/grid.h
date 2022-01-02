/*
 * grid.h
 *
 *  Created on: 02.01.2022
 *      Author: farnsworth
 */

#ifndef SRC_GRID_H_
#define SRC_GRID_H_

#include "geometries.h"
#include "Image3f.h"

#include<tuple>

// erstmal anzeigen, wie eine box?
class Grid : public IIntersectable {
	glm::vec3 start_pos; // left, top = x_min, y_min, z_min
	glm::vec3 end_pos; 	 // position + size x_max, y_max, z_max
	glm::vec3 size; 	 // w, h, d
	glm::vec3 resolution; // resolution

	std::vector<IIntersectable*>* cells;		// cells of vectors

	std::pair<glm::vec3, glm::vec3 > getSceneBounds(std::vector<IIntersectable*> *geometries_ptr) {
		// get bounds
		glm::vec3 min_start = glm::vec3(1, 1, 1) * FLOAT_MAX;
		glm::vec3 max_end = glm::vec3(1, 1, 1) * FLOAT_MIN;
		for(auto const& geometry_ptr : *geometries_ptr) {
			auto [start, end] = geometry_ptr->getExtends();
			min_start = glm::min(glm::min(min_start, start), end);
			max_end = glm::max(glm::max(max_end, end), start);
		}

		const float epsilon = 0.001;
		glm::vec3 epsilon_vec = glm::vec3(1, 1, 1) * epsilon;
		return std::pair<glm::vec3, glm::vec3> {min_start - epsilon_vec, max_end + epsilon_vec};
	}

	Grid(std::vector<IIntersectable*> *geometries_ptr) {		//glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 resolution)  {
		glm::vec3 resolution = glm::vec3(5, 5, 5);
		auto [start, end] = this->getSceneBounds(geometries_ptr);
		this->start_pos = start;
		this->end_pos = end;
		this->size = end - start;
		this->resolution = resolution;

		cells = new std::vector<IIntersectable>[resolution.x * resolution.y * resolution.z];

		for(auto const& geometry_ptr : geometries_ptr) {
            this->placeIntoGrid(geometry_ptr);
		}
	}

	Grid(glm::vec3 start, glm::vec3 end) {		//glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 resolution)  {
		glm::vec3 resolution = glm::vec3(5, 5, 5);
		this->start_pos = start;
		this->end_pos = end;
		this->size = end - start;
		this->resolution = resolution;

		cells = new std::vector<IIntersectable>[resolution.x * resolution.y * resolution.z];
	}

	~Grid() {
		delete cells;
	}

	inline std::tuple<int, int, int> getCellIndicesAtPosition(glm::vec3 position) {
		int ix = clamp(glm::floor(position.x - start_pos.x) * resolution.x / size.x, 0, resolution.x - 1);
		int iy = clamp(glm::floor(position.y - start_pos.y) * resolution.y / size.y, 0, resolution.y - 1);
		int iz = clamp(glm::floor(position.z - start_pos.z) * resolution.z / size.z, 0, resolution.z - 1);

		return ix, iy, iz;
	};

	inline int getOffsetAtIndices(int index_x, int index_y, int index_z) {
		return index_x + this->resolution.x * index_y + this->resolution.y * this->resolution.x * index_z;
	};

	void placeIntoCell(int index_x, int index_y, int index_z, IIntersectable *geometry_ptr) {
		auto offset = this->getOffsetAtIndices(index_x, index_y, index_z);
		this->cells[offset].push_back(geometry_ptr);
	};

	std::vector<IIntersectable>* getCellAtIndices(int index_x, int index_y, int index_z, IIntersectable *geometry_ptr) {
		auto offset = this->getOffsetAtIndices(index_x, index_y, index_z);
		return &this->cells[offset];
	};

	// Places geometry into grid cells. Extends won't be changed and should already exist.
	void placeIntoGrid(IIntersectable *geometry_ptr)  {
		auto [start, end] = geometry_ptr->getExtends();
		auto [ix_min, iy_min, iz_min] = this->getCellIndicesAtPosition(start);
		auto [ix_max, iy_max, iz_max]= this->getCellIndicesAtPosition(end);

        for (int index_z = iz_min; index_z <= iz_max; index_z++) {
            for (int index_y = iy_min; index_y <= iy_max; index_y++) {
                for (int index_x = ix_min; index_x <= ix_max; index_x++) {
                    this->placeIntoCell(index_x, index_y, index_z, geometry_ptr);
                }
            }
        }
	};

	// calculates intersection with grid bbox and returns cell strides for grid on rayDir for scalar t (dtx, dty, dtz)
	std::tuple<bool, float, float, float> collidesWithBox(glm::vec3 rayOrigin, glm::vec3 rayDir) {
		const float epsilon = 0.0001;
		// t.._start and t.._end are ray intersections with bounding box planes (axis aligned)
		// division by zero supposedly treated as inf, -inf (IEEE floating point spec)
		float tx_start = (this->start_pos.x - rayOrigin.x) / rayDir.x;
		float tx_end = (this->end_pos.x - rayOrigin.x) / rayDir.x;

		float ty_start = (this->start_pos.y - rayOrigin.y) / rayDir.y;
		float ty_end = (this->end_pos.y - rayOrigin.y) / rayDir.y;

		float tz_start = (this->start_pos.z - rayOrigin.z) / rayDir.z;
		float tz_end = (this->end_pos.z - rayOrigin.z) / rayDir.z;

		// t.._min, t.._max are length sorted hit points in relation to ray scalar
		// TODO: might be more performant to assign with cases rather than by default
		float tx_min = tx_start;
		float tx_max = tx_end;
		if(rayDir.x < 0) {
			tx_min = tx_end;
			tx_max = tx_start;
		}

		float ty_min = ty_start;
		float ty_max = ty_end;
		if(rayDir.y < 0) {
			ty_min = ty_end;
			ty_max = ty_start;
		}

		float tz_min = tz_start;
		float tz_max = tz_end;
		if(rayDir.z < 0) {
			tz_min = tz_end;
			tz_max = tz_start;
		}

		// t0, t1 interval of interesections on ray
		float t0 = std::max({tx_min, ty_min, tz_min});
		float t1 = std::min({tx_max, ty_max, tz_max});

		// TODO: convenience calcs and returns
		float dtx = (tx_max - tx_min) / this->resolution.x;
		float dty = (ty_max - ty_min) / this->resolution.y;
		float dtz = (tz_max - tz_min) / this->resolution.z;

		return {t0 < t1 && t1 > epsilon, dtx, dty, dtz};
	};

	virtual HitInfo intersect(glm::vec3 O, glm::vec3 D) {
		return HitInfo();
	};

	virtual std::pair<glm::vec3, glm::vec3> getExtends() {
		return {this->start_pos, this->end_pos};
	};
};



#endif /* SRC_GRID_H_ */

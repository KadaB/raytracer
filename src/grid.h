

#ifndef SRC_GRID_H_
#define SRC_GRID_H_

#include "geometries.h"
#include "Image3f.h"

#include<tuple>

class Grid : public IIntersectable {
	glm::vec3 start_pos; // lowest bounds in aabb
	glm::vec3 end_pos; 	 // highest bounds ind aabb
	glm::vec3 size; 	 // w, h, d
	glm::vec3 resolution;// grid resolution

	std::unique_ptr<Container[]> cells;		// cells of vectors

public:

	std::pair<glm::vec3, glm::vec3 > getSceneBounds(std::vector<ITransformedIntersectable*> *geometries_ptr) {
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

	Grid(std::vector<ITransformedIntersectable*> *geometries_ptr) {		//glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 resolution)  {
		glm::vec3 resolution = glm::vec3(1, 1, 1) * 15.0f;
		auto [start, end] = this->getSceneBounds(geometries_ptr);
		this->start_pos = start;
		this->end_pos = end;
		this->size = end - start;
		this->resolution = resolution;

		cells = std::make_unique<Container[]>(int(resolution.x) * int(resolution.y) * int(resolution.z));

		for(auto const& geometry_ptr : *geometries_ptr) {
            this->placeIntoGrid(geometry_ptr);
		}
	}

	~Grid() { }

	inline std::tuple<int, int, int> getCellIndicesAtPosition(glm::vec3 position) {
		int ix = clamp(glm::floor((position.x - start_pos.x) * resolution.x / size.x), 0, resolution.x - 1);
		int iy = clamp(glm::floor((position.y - start_pos.y) * resolution.y / size.y), 0, resolution.y - 1);
		int iz = clamp(glm::floor((position.z - start_pos.z) * resolution.z / size.z), 0, resolution.z - 1);

		return {ix, iy, iz};
	};

	inline int getOffsetAtIndices(int index_x, int index_y, int index_z) {
		return index_x + this->resolution.x * index_y + this->resolution.y * this->resolution.x * index_z;
	};

	void placeIntoCell(int index_x, int index_y, int index_z, ITransformedIntersectable *geometry_ptr) {
		auto offset = this->getOffsetAtIndices(index_x, index_y, index_z);
		this->cells.get()[offset].add(geometry_ptr);
	};

	Container* getCellAtIndices(int index_x, int index_y, int index_z) {
		auto offset = this->getOffsetAtIndices(index_x, index_y, index_z);
		return &this->cells.get()[offset];
	};

	// Places geometry into grid cells. Extends won't be changed and should already exist.
	void placeIntoGrid(ITransformedIntersectable *geometry_ptr)  {
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
	std::tuple<bool, float, glm::vec3, glm::vec3> collidesWithBox(glm::vec3 rayOrigin, glm::vec3 rayDir) {
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
		glm::vec3 delta_t = glm::vec3 ( (tx_max - tx_min) / this->resolution.x,
										(ty_max - ty_min) / this->resolution.y,
										(tz_max - tz_min) / this->resolution.z);
		glm::vec3 t_min = glm::vec3(tx_min, ty_min, tz_min);

		return {t0 < t1 && t1 > epsilon, t0, t_min, delta_t};
	};

	bool isInsideGrid(glm::vec3 position) {
		if(position.x > this->start_pos.x && position.x < this->end_pos.x &&
           position.y > this->start_pos.y && position.y < this->end_pos.y &&
           position.z > this->start_pos.z && position.z < this->end_pos.z) {
			return true;
		}
		return false;
	}

	FragmentInfo traverseGrid(glm::vec3 rayOrigin, glm::vec3 rayDir, float t_limit) {
		auto [ isHit, t, t_mins, dt ] = this->collidesWithBox(rayOrigin, rayDir);

		if(!isHit) {
			return FragmentInfo();
		}

		glm::vec3 position;
		if(this->isInsideGrid(rayOrigin)) {
			position = rayOrigin;
		}
		else {
			position = rayOrigin + t * rayDir;
		}

		auto [ index_x, index_y, index_z ] = this->getCellIndicesAtPosition(position);

		int ix_step;
		int ix_stop;
		float tx_next;
		if(rayDir.x > 0) {
            ix_step = 1;
            ix_stop = int(this->resolution.x);
            int next_ix = index_x + 1;
            tx_next = t_mins.x + next_ix * dt.x;
		}
		else if (rayDir.x < 0) {
            ix_step = -1;
            ix_stop = -1;
            int next_ix = int(this->resolution.x) - index_x;
            tx_next = t_mins.x + next_ix * dt.x;
		}
		else {
            ix_step = -1;
            ix_stop = -1;
            tx_next = FLOAT_MAX;
		}

		int iy_step;
		int iy_stop;
		float ty_next;
		if(rayDir.y > 0) {
            iy_step = 1;
            iy_stop = int(this->resolution.y);
            int next_iy = index_y + 1;
            ty_next = t_mins.y + next_iy * dt.y;
		}
		else if (rayDir.y < 0) {
            iy_step = -1;
            iy_stop = -1;
            int next_iy = int(this->resolution.y) - index_y;
            ty_next = t_mins.y + next_iy * dt.y;
		}
		else {
            iy_step = -1;
            iy_stop = -1;
            ty_next = FLOAT_MAX;
		}

		int iz_step;
		int iz_stop;
		float tz_next;
		if(rayDir.z > 0) {
            iz_step = 1;
            iz_stop = int(this->resolution.z);
            int next_iz = index_z + 1;
            tz_next = t_mins.z + next_iz * dt.z;
		}
		else if (rayDir.z < 0) {
            iz_step = -1;
            iz_stop = -1;
            int next_iz = int(this->resolution.z) - index_z;
            tz_next = t_mins.z + next_iz * dt.z;
		}
		else {
            iz_step = -1;
            iz_stop = -1;
            tz_next = FLOAT_MAX;
		}

		while(index_x != ix_stop && index_y != iy_stop && index_z != iz_stop) {
			float t_next_min = std::min({tx_next, ty_next, tz_next, t_limit}); // readability/convenience

			Container *cell = this->getCellAtIndices(index_x, index_y, index_z);
			FragmentInfo fragmentInfo = cell->intersect(rayOrigin, rayDir, t_next_min);

			if(fragmentInfo.validHit) {
				return fragmentInfo;
			}

			if(t_next_min == tx_next) {
				index_x += ix_step;
				tx_next += dt.x;
			}
			else if(t_next_min == ty_next) {
				index_y += iy_step;
				ty_next += dt.y;
			}
			else if(t_next_min == tz_next) {
				index_z += iz_step;
				tz_next += dt.z;
			}
			else if(t_next_min == t_limit) {
				return FragmentInfo();
			}
		}

		return FragmentInfo();
	}

	virtual FragmentInfo intersect(glm::vec3 O, glm::vec3 D, float t_limit = FLT_MAX) {
		return this->traverseGrid(O, D, t_limit);
	};

	virtual std::pair<glm::vec3, glm::vec3> getExtends() {
		return {this->start_pos, this->end_pos};
	};
};



#endif /* SRC_GRID_H_ */

#pragma once
#include "mesh.h"
#include "shape.h"

KRR_NAMESPACE_BEGIN

AABB Mesh::computeBoundingBox() {
	aabb = {};
	for (const auto &v : positions) 
		aabb.extend(v);
	return aabb;
}

KRR_NAMESPACE_END
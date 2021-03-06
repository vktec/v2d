#include <math.h>
#include <stdbool.h>
#include "v2d/collide.h"
#include "v2d/vector.h"

// Clamp a scalar to between two others
static inline double _clamp(double a, double min, double max) {
	if (min < max) return fmax(min, fmin(max, a));
	else return fmax(max, fmin(min, a));
}

// Clamp a vector to between two others
static inline v2d_vec_t _clampv(v2d_vec_t a, v2d_vec_t min, v2d_vec_t max) {
	return v2d_vec(
		_clamp(v2dvx(a), v2dvx(min), v2dvx(max)),
		_clamp(v2dvy(a), v2dvy(min), v2dvy(max))
	);
}

// Flip a rect's dimensions if they are negative
static v2d_rect_t _rect_fix(v2d_rect_t r) {
	double *p = v2d_vec_a(r.pos), *d = v2d_vec_a(r.dim);

	// Hopefully the compiler will be nice and unroll this loop for us
	for (int i = 0; i < 2; i++) {
		// Check if the dimension is negative
		if (d[i] < 0) {
			d[i] = -d[i]; // Fix the sign
			p[i] -= d[i]; // Fix the position
		}
	}

	return r;
}

bool v2d_collide_point_circle(v2d_vec_t p, v2d_circle_t c) {
	return v2d_vec_mag2(c.pos - p) < c.rad*c.rad;
}

bool v2d_collide_point_rect(v2d_vec_t p, v2d_rect_t b) {
	b = _rect_fix(b);
	v2d_vec_t min = b.pos, max = b.pos + b.dim;

	if (v2dvx(p) < v2dvx(min)) return false;
	if (v2dvx(p) > v2dvx(max)) return false;
	if (v2dvy(p) < v2dvy(min)) return false;
	if (v2dvy(p) > v2dvy(max)) return false;

	return true;
}

bool v2d_collide_circle_circle(v2d_circle_t a, v2d_circle_t b) {
	// The distance the two circles need to be within to collide
	double d = a.rad + b.rad;
	// We compare squares because that's faster
	return v2d_vec_mag2(a.pos - b.pos) < d*d;
}

bool v2d_collide_rect_rect(v2d_rect_t a, v2d_rect_t b) {
	// Ensure a and b have positive dimensions
	a = _rect_fix(a);
	b = _rect_fix(b);

	// Extract the bottom left and top right points
	v2d_vec_t amin = a.pos, amax = a.pos + a.dim;
	v2d_vec_t bmin = b.pos, bmax = b.pos + b.dim;

	// a is to the left of b
	if (v2dvx(amax) < v2dvx(bmin)) return false;
	// b is to the left of a
	if (v2dvx(bmax) < v2dvx(amin)) return false;
	// a is below b
	if (v2dvy(amax) < v2dvy(bmin)) return false;
	// b is below a
	if (v2dvy(bmax) < v2dvy(amin)) return false;

	// If none of those are true, we're colliding
	return true;
}

bool v2d_collide_circle_rect(v2d_circle_t a, v2d_rect_t b) {
	v2d_vec_t min = b.pos, max = b.pos + b.dim;
	return v2d_vec_mag2(_clampv(a.pos, min, max) - a.pos) < a.rad*a.rad;
}

double v2d_raycast_circle(v2d_ray_t r, v2d_circle_t c) {
	// Shortcut
	if (v2d_collide_point_circle(r.pos, c)) return 0;
	if (r.dir == 0) return INFINITY;
	if (v2d_collide_point_circle(r.pos + r.dir, c)) return 0;

	// Translate the ray's position to origin
	v2d_vec_t cpos = c.pos - r.pos;

	// Compute this once, we'll need it a few times
	double rmag = v2d_vec_mag(r.dir);
	double rimag = 1/rmag;

	// Project the center point of the circle onto the ray
	// This is an actual distance, not a λ value
	double proj = v2d_vec_dot(cpos, r.dir) * rimag;

	// Make sure we don't go outside of the ray
	if (proj < 0) proj = 0;
	else if (proj > rmag) proj = rmag;

	// Otherwise, check the distance from the projected point to the circle's center
	double distance = v2d_vec_mag2(cpos - r.dir*proj*rimag);
	// If it's more than the circle's radius, there's no collision
	if (distance >= c.rad*c.rad) return INFINITY;

	// There's a collision. Find the point at which it occurs

	// The distance from the center to the collision point is `c.rad`
	// The distance from the center to the projected point is `sqrt(distance)`
	// Thus, the distance from the projected point to the collision point is `sqrt(c.rad*c.rad - distance)`
	// If this value is subtracted from `proj`, it produces the distance of the collision point from r.pos
	// We then need to divide by v2d_vec_mag(r.dir) to get the value for λ that we need to return
	double h = (proj - sqrt(c.rad*c.rad - distance)) * rimag;
	if (0 <= h && h <= 1) return h;
	return INFINITY;
}

double v2d_raycast_rect(v2d_ray_t r, v2d_rect_t b) {
	// This uses the "slab" method of ray-AABB intersection

	// Shortcut
	if (v2d_collide_point_rect(r.pos, b)) return 0;
	if (r.dir == 0) return INFINITY;
	if (v2d_collide_point_rect(r.pos + r.dir, b)) return 0;

	// The two corners of the rect
	b = _rect_fix(b);
	v2d_vec_t bmin = b.pos, bmax = b.pos + b.dim;

	// These are the min and max values for λ (named h here for simplicity)
	double hmin, hmax;

	// Chop at the left and right x coordinates
	double xcoef = 1/v2dvx(r.dir);
	double hx1 = (v2dvx(bmin) - v2dvx(r.pos)) * xcoef;
	double hx2 = (v2dvx(bmax) - v2dvx(r.pos)) * xcoef;
	hmax = fmax(hx1, hx2);
	hmin = fmin(hx1, hx2);

	// Exit early if there's already no intersection
	if (hmax < hmin) return INFINITY;

	// Chop at the top and bottom y coordinates
	double ycoef = 1/v2dvy(r.dir);
	double hy1 = (v2dvy(bmin) - v2dvy(r.pos)) * ycoef;
	double hy2 = (v2dvy(bmax) - v2dvy(r.pos)) * ycoef;
	hmax = fmin(hmax, fmax(hy1, hy2));
	hmin = fmax(hmin, fmin(hy1, hy2));

	if (hmax < hmin) return INFINITY;
	double h = hmin < 0 ? hmax : hmin;
	if (0 <= h && h <= 1) return h;
	return INFINITY;
}

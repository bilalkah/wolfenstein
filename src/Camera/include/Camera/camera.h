/**
 * @file camera.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_INCLUDE_CAMERA_H
#define CAMERA_INCLUDE_CAMERA_H

#include "Camera/ray.h"
#include "Camera/raycaster.h"
#include "GameObjects/game_object.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace wolfenstein {

struct Camera2DConfig
{
	Camera2DConfig(int width, double fov, double depth)
		: width(width), fov(fov), depth(depth) {}

	int width;
	double fov;
	double depth;
};

using RayPair = std::pair<Ray, Ray>;

class Scene;
class Camera2D
{
  public:
	explicit Camera2D(const Camera2DConfig& config);

	// Casts the view from `eye` and places every object `alpha` of the way
	// from its previous simulation tick to the latest; once per drawn frame
	void Update(const Position2D& eye, double alpha);

	// Borrows the scene until the next call; the game owns it
	void SetScene(Scene& scene);
	// Marks what the view shows as explored in the scene: the floor the rays
	// cross, the walls they end on, and the cells around the eye
	void ExploreView();
	// Makes room for this many objects' views up front, so SetScene never
	// grows them
	void ReserveViews(std::size_t objects) { views_.reserve(objects); }
	const RayVector& GetRays() const;
	// The centre ray and what it points at, for drawing only: shots are
	// resolved by the simulation (Aim), not from the view
	const Ray& GetCrosshairRay() const { return crosshair_ray_; }
	// An object as the eye sees it this frame: the rays bounding it, how far
	// in front of the eye its centre stands (along the view: what it is drawn
	// at), whether its picture is mirrored (a body lying dead, seen from
	// behind), and its tint (another player's colour)
	struct Sight
	{
		RayPair rays;
		double distance = 0.0;
		bool mirrored = false;
		IGameObject::Tint tint{};
	};
	// How the eye sees an object in the current frame, or nullptr if it is
	// not in view
	const Sight* FindObject(ObjectId id) const;
	const Position2D& GetPosition() const { return eye_; }
	// How far the view is tipped up (+) or down: the player's look and a
	// shot's kick, as a share of the screen's height (Player::GetPitch)
	void SetPitch(double pitch) { pitch_ = pitch; }
	double GetPitch() const { return pitch_; }
	double GetFov() const;
	// Widens or narrows the view to `fov` radians: as many rays, spread over
	// a wider plane
	void SetFov(double fov);
	// Where a direction `camera_angle` off the view's centre falls across
	// the screen, from -1 (left edge) to 1 (right edge): the rays' own
	// projection, so sprites line up with the walls
	double Across(double camera_angle) const;

  private:
	void InitRays();
	void Calculate(const IGameObject& object, double alpha);
	double WorldAngleToCameraAngle(double angle) const;

	Camera2DConfig config_;
	Scene* scene_ = nullptr;
	Position2D eye_;
	double pitch_ = 0.0;
	Ray crosshair_ray_;
	RayCaster ray_cast_;
	RayVector rays_;
	// Rays of each object, indexed by ObjectId and sized once per level (in
	// SetScene); an entry is current only if it was written in this frame
	struct ObjectView
	{
		Sight sight;
		std::uint64_t frame = std::numeric_limits<std::uint64_t>::max();
	};
	std::vector<ObjectView> views_;
	std::uint64_t frame_ = 0;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_H
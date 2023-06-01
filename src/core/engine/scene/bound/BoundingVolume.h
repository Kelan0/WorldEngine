#ifndef WORLDENGINE_BOUNDINGVOLUME_H
#define WORLDENGINE_BOUNDINGVOLUME_H

#include "core/core.h"

class BoundingVolume {
public:
    enum Type : uint8_t {
        Type_Sphere = 0,
        Type_AxisAlignedBoundingBox = 1,
        Type_OrientedBoundingBox = 2,
        Type_Cylinder = 3,
        Type_Capsule = 4,
    };

public:
    explicit BoundingVolume(Type type);

    virtual ~BoundingVolume() = 0;

    virtual glm::dvec3 getCenter() const = 0;

    /**
     * Check if this bounding volume intersects with another bounding volume. This will also be true if
     * either contains the other.
     * @param other The other bounding volume to check for an intersection with
     * @return true if there is any overlap (the overlapping volume is greater than zero)
     */
    virtual bool intersects(const BoundingVolume& other) const = 0;

    /**
     * Check if this bounding volume fully contains another bounding volume. If any of the other bounding
     * volume overlaps with the boundary of this bounding volume, it is considered not to be contained
     * within this bounding volume.
     * @param other The other bounding volume to check if is contained fully within this bounding volume
     * @return true if this bounding volume fully contains the other
     */
    virtual bool contains(const BoundingVolume& other) const = 0;

    /**
     * Check if this bounding volume contains a given point
     * @param other The point to check if is contained within this bounding volume
     * @return true if the point is contained within this bounding volume
     */
    virtual bool contains(const glm::dvec3& other) const = 0;

    /**
     * Calculate the smallest distance between this bounding volume and another bounding volume (i.e. the
     * distance between the closest point on the surface of this volume to the other, and the closest
     * point on the surface of the other volume to this)
     * @param other The other bounding volume to find the minimum distance to.
     * @return The distance between the closest points on both volumes.
     */
    virtual double calculateMinDistance(const BoundingVolume& other) const = 0;

    /**
     * Calculate the smallest distance between this bounding volume and a given point. (i.e. the distance
     * between the closest point on the surface of this volume to the given point)
     * @param other The point to calculate the minimum distance to
     * @return The distance between the closest point on this volume and the given point.
     */
    virtual double calculateMinDistance(const glm::dvec3& other) const = 0;

    /**
     * Calculate the closest point on the surface of this volume to the given point
     * @param point The point to calculate the closest surface point to.
     * @return The closest point on the surface of this volume to the given point
     */
    virtual glm::dvec3 calculateClosestPoint(const glm::dvec3& point) const = 0;

    Type getType() const;

    template<typename T>
    static const T& cast(const BoundingVolume& bv);

private:
    Type m_type;
};




class BoundingSphere : public BoundingVolume {
public:
    BoundingSphere(const glm::dvec3& center, double radius);
    BoundingSphere(double centerX, double centerY, double centerZ, double radius);

    ~BoundingSphere() override;

    glm::dvec3 getCenter() const override;

    void setCenter(const glm::dvec3& center);

    void setCenter(double centerX, double centerY, double centerZ);

    double getRadius() const;

    void setRadius(double radius);

    bool intersects(const BoundingVolume& other) const override;

    bool contains(const BoundingVolume& other) const override;

    bool contains(const glm::dvec3& other) const override;

    double calculateMinDistance(const BoundingVolume& other) const override;

    double calculateMinDistance(const glm::dvec3& other) const override;

    glm::dvec3 calculateClosestPoint(const glm::dvec3& point) const override;

    void drawLines() const;

    void drawFill() const;

private:
    glm::dvec3 m_center;
    double m_radius;
};

class AxisAlignedBoundingBox : public BoundingVolume {
public:
    enum {
        Axis_X = 0b100,
        Axis_Y = 0b010,
        Axis_Z = 0b001,
    };
    enum {
        Corner_X0_Y0_Z0 = 0b000,
        Corner_X0_Y0_Z1 = 0b001,
        Corner_X0_Y1_Z0 = 0b010,
        Corner_X0_Y1_Z1 = 0b011,
        Corner_X1_Y0_Z0 = 0b100,
        Corner_X1_Y0_Z1 = 0b101,
        Corner_X1_Y1_Z0 = 0b110,
        Corner_X1_Y1_Z1 = 0b111,
    };
public:
    AxisAlignedBoundingBox();

    AxisAlignedBoundingBox(const glm::dvec3& center, const glm::dvec3& halfExtents);

    ~AxisAlignedBoundingBox() override;

    glm::dvec3 getCenter() const override;

    void setCenter(const glm::dvec3& center);

    void setCenter(double centerX, double centerY, double centerZ);

    const glm::dvec3& getHalfExtents() const;

    void setHalfExtents(const glm::dvec3& halfExtents);

    void setHalfExtents(double halfExtentX, double halfExtentY, double halfExtentZ);

    glm::dvec3 getBoundMin() const;
    double getBoundMinX() const;
    double getBoundMinY() const;
    double getBoundMinZ() const;

    glm::dvec3 getBoundMax() const;
    double getBoundMaxX() const;
    double getBoundMaxY() const;
    double getBoundMaxZ() const;

    void setBoundMinMax(const glm::dvec3& boundMin, const glm::dvec3& boundMax);

    glm::dvec3 getCorner(int cornerIndex) const;

    std::array<glm::dvec3, 8> getCorners() const;

    bool intersects(const BoundingVolume& other) const override;

    bool contains(const BoundingVolume& other) const override;

    bool contains(const glm::dvec3& other) const override;

    double calculateMinDistance(const BoundingVolume& other) const override;

    double calculateMinDistance(const glm::dvec3& other) const override;

    glm::dvec3 calculateClosestPoint(const glm::dvec3& point) const override;

    void drawLines() const;

    void drawFill() const;

private:
    glm::dvec3 m_center;
    glm::dvec3 m_halfExtents;
};



template<typename T>
const T& BoundingVolume::cast(const BoundingVolume& bv) {
#if _DEBUG
    return dynamic_cast<const T&>(bv);
#else
    return static_cast<const T&>(bv);
#endif
}

#endif //WORLDENGINE_BOUNDINGVOLUME_H

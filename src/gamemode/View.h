#pragma once

#include "Types.h"

class CWorld;

class CView {
public:
    struct CameraConstraints {
        float minDistance;
        float maxDistance;
        float defaultDistance;
        float minLatitude;
        float maxLatitude;
        float defaultLatitude;
        float defaultLongitude;
        bool lockLongitude;
        bool constrainLongitude;
        float minLongitude;
        float maxLongitude;
    };

    CView();
    ~CView();

    void SetWorld(CWorld* world);
    void SetCameraConstraints(const CameraConstraints& constraints);
    void SetInitialCamera(float longitude, float latitude, float distance);
    void OnCalcViewInfo();
    void OnEnterFrame();
    void OnExitFrame();
    void OnRender();
    void AddDistance(float delta);
    void AddLatitude(float delta);
    void AddLongitude(float delta);
    void RotateByDrag(int deltaX, int deltaY);
    void ZoomByWheel(int wheelDelta);
    void ResetToDefaultOrientation();
    void UpdateHoverCellFromScreen(int screenX, int screenY);
    void ClearHoverCell();
    bool ScreenToHoveredAttrCell(int screenX, int screenY, int* outAttrX, int* outAttrY) const;
    const matrix& GetViewMatrix() const { return m_viewMatrix; }
    float GetCameraLongitude() const { return m_cur.longitude; }
    float GetTargetCameraLatitude() const { return m_dest.latitude; }
    float GetTargetCameraDistance() const { return m_dest.distance; }

private:
    struct CameraState {
        vector3d at;
        float distance;
        float longitude;
        float latitude;
    };

    void BuildViewMatrix();
    void InterpolateViewInfo();
    void ClampCameraState(CameraState* state) const;
    float ClampLongitude(float longitude) const;
    vector3d ResolveTargetPosition() const;
    bool ScreenToAttrCell(int screenX, int screenY, int* outAttrX, int* outAttrY) const;

    CWorld* m_world;
    CameraConstraints m_constraints;
    CameraState m_cur;
    CameraState m_dest;
    vector3d m_from;
    vector3d m_up;
    matrix m_viewMatrix;
    matrix m_invViewMatrix;
    bool m_initialized;
    int m_resetBlendFrames;
    int m_hoverAttrX;
    int m_hoverAttrY;
};
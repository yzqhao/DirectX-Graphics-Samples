//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pch.h"
#include "Camera.h"
#include <cmath>

using namespace Math;

void BaseCamera::SetLookDirection( Vector3 forward, Vector3 up )
{
	if (m_bDefine)
	{
        forward = Normalize(forward);
		Math::Vector3 R = Normalize(Cross(up, forward));
		Math::Vector3 U = Normalize(Cross(forward, R));

        U = Normalize(Cross(forward, R));
        R = Normalize(Cross(U, forward));

		m_Basis = Matrix3(R, up, forward);
        //m_Basis = Invert(Matrix4(m_Basis)).Get3x3();
		m_CameraToWorld.SetRotation(Quaternion(m_Basis) * Quaternion(Math::XM_PI, 0, Math::XM_PI));
        return;
	}
    // Given, but ensure normalization
    Scalar forwardLenSq = LengthSquare(forward);
    forward = Select(forward * RecipSqrt(forwardLenSq), -Vector3(kZUnitVector), forwardLenSq < Scalar(0.000001f));

    // Deduce a valid, orthogonal right vector
    Vector3 right = Cross(forward, up);
    Scalar rightLenSq = LengthSquare(right);
    right = Select(right * RecipSqrt(rightLenSq), Quaternion(Vector3(kYUnitVector), -XM_PIDIV2) * forward, rightLenSq < Scalar(0.000001f));

    // Compute actual up vector
    up = Cross(right, forward);

    // Finish constructing basis
    m_Basis = Matrix3(right, up, -forward);
    m_CameraToWorld.SetRotation(Quaternion(m_Basis));
}

void BaseCamera::Update()
{
    m_PreviousViewProjMatrix = m_ViewProjMatrix;

    m_ViewMatrix = Matrix4(~m_CameraToWorld);
    m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
    m_ReprojectMatrix = m_PreviousViewProjMatrix * Invert(GetViewProjMatrix());

    m_FrustumVS = Frustum( m_ProjMatrix );
    m_FrustumWS = m_CameraToWorld * m_FrustumVS;
}


void Camera::UpdateProjMatrix( void )
{
    /*
    if (m_bDefine)
	{
        SetProjMatrix( Matrix4(
        Vector4( 1, 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, 1.777777779, 0.0f, 0.0f ),
        Vector4( 0.0f, 0.0f, 1.00009990, 1.0f ),
        Vector4( 0.0f, 0.0f, -0.111, 0.0f )
        ) );

        return;
    }
    */
    
    float Y = 1.0f / std::tanf( m_VerticalFOV * 0.5f );
    float X = Y * m_AspectRatio;

    if (m_bDefine)
    {
        X = 1.0f / std::tanf(m_VerticalFOV);
        Y = X / m_AspectRatio;
    }

    float Q1, Q2;

    // ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
    // actually a great idea with F32 depth buffers to redistribute precision more evenly across
    // the entire range.  It requires clearing Z to 0.0f and using a GREATER variant depth test.
    // Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
    if (m_ReverseZ)
    {
        if (m_InfiniteZ)
        {
            Q1 = 0.0f;
            Q2 = m_NearClip;
        }
        else
        {
            Q1 = m_NearClip / (m_FarClip - m_NearClip);
            Q2 = Q1 * m_FarClip;
        }
    }
    else
    {
        if (m_InfiniteZ)
        {
            Q1 = -1.0f;
            Q2 = -m_NearClip;
        }
        else
        {
            Q1 = m_FarClip / (m_NearClip - m_FarClip);
            Q2 = Q1 * m_NearClip;
        }

		if (m_bDefine)
		{
			Q1 = m_FarClip / (m_FarClip - m_NearClip);
			Q2 = Q1 * -m_NearClip;
		}
    }

    float Q3 = m_bDefine ? 1.0f : -1.0f;

    SetProjMatrix( Matrix4(
        Vector4( X, 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, Y, 0.0f, 0.0f ),
        Vector4( 0.0f, 0.0f, Q1, Q3),
        Vector4( 0.0f, 0.0f, Q2, 0.0f )
        ) );
}

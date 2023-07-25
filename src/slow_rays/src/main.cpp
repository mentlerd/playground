
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
 
#if defined(__APPLE__)                                                                                                                                                                                                            
#include <OpenGL/gl.h>                                                                                                                                                                                                            
#include <OpenGL/glu.h>                                                                                                                                                                                                           
#include <GLUT/glut.h>                                                                                                                                                                                                            
#else                                                                                                                                                                                                                             
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)                                                                                                                                                                       
#include <windows.h>                                                                                                                                                                                                              
#endif                                                                                                                                                                                                                            
#include <GL/gl.h>                                                                                                                                                                                                                
#include <GL/glu.h>                                                                                                                                                                                                               
#include <GL/glut.h>                                                                                                                                                                                                              
#endif          
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define PI M_PI

float rOff = 0.01;
 
struct Vector {
	float x, y, z;
 
	Vector() : x(0), y(0), z(0) 
	{}
	Vector(float x, float y, float z) : x(x), y(y), z(z)
	{}
 
	float  operator*(const Vector& v) const {
		return (x * v.x + y * v.y + z * v.z);
	}
 
	Vector operator%(const Vector& v) const {
		return Vector(y*v.z-z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
	}
 
	Vector operator*(float s) const { return Vector(x *s, y *s, z *s); }
	Vector operator/(float s) const { return Vector(x /s, y /s, z /s); }
 
	Vector operator+(const Vector& v) const { return Vector(x + v.x, y + v.y, z + v.z); }
	Vector operator-(const Vector& v) const { return Vector(x - v.x, y - v.y, z - v.z); }
 
	Vector operator-() const { return Vector(-x, -y, -z); }
 
	Vector normal() const { 
		float s = len();
 
		return Vector(x /s, y /s, z /s);
	}
 
	float len() const { 
		return sqrt(x*x + y*y + z*z); 
	}
};
struct Color {
	float r, g, b;
 
	Color( float i = 0 ) : r(i), g(i), b(i)
	{}
 
	Color( float r, float g, float b ) : r(r), g(g), b(b)
	{}
 
	Color operator*( float s ) const {
		return Color(r * s, g * s, b * s);
	}
	Color operator*( const Color& c ) const {
		return Color(r * c.r, g * c.g, b * c.b);
	}
 
	Color operator/( const Color& c ) const {
		return Color(r / c.r, g / c.g, b / c.b);    
	}
 
	Color operator+( const Color& c ) const {
		return Color(r + c.r, g + c.g, b + c.b);
	}
	Color operator-( const Color& c ) const {
		return Color(r - c.r, g - c.g, b - c.b);
	}
};
 
 
class SceneObj;
 
struct Ray {
	float T;
	float C;
 
	Vector origin;
	Vector dir;
};
 
struct HitRes {
	Ray ray;
 
	SceneObj* obj;
 
	Vector pos;
	Vector normal;
 
	float frac;
 
	float u;
	float v;

	HitRes( const Ray& ray ) : 
		ray(ray), 
 
		obj(NULL),
		frac(-1),
 
		u(0),
		v(0)
	{}
 
	float getT() const {
		return ray.T - (frac / ray.C);
	}

	Ray createRay( Vector dir ) const {
		return createRay(dir, ray.C);
	}
 
	Ray createRay( Vector dir, float C ) const {
		Ray out;
		 out.T      = getT();
		 out.C      = C;
 
		 out.dir    = dir;
		 out.origin = pos + dir * rOff;
 
		return out;
	}
};
 
int MAT_SHADOW_CASTER     = (1 << 0);
int MAT_REFLECT         = (1 << 1); 
int MAT_REFRACT         = (1 << 2);
 
class Material {
	public:
		int flags;
 
		Material( int flags ) :
			flags(flags)
		{}
 
		virtual Color shade( Vector normal, Vector viewDir, Vector lightDir, Color rad ) const {
			return Color(0);
		}
		virtual Color Freshnel( Vector dir, Vector normal ) const {
			return Color(0);
		}
 
		virtual Ray reflect( const HitRes& res ){
			return Ray();
		}
		virtual Ray refract( const HitRes& res ){
			return Ray();
		}
};
 
// Forrás: VIK Wiki, F0 approximáció
Color calcFreshnelF0( Color n, Color k ){
	return ((n-1)*(n-1) + k*k) / ((n+1)*(n+1) + k*k);
}
 
class SmoothMaterial : public Material {
	public:
		Color F0;
		float n;
 
		SmoothMaterial( float n, Color k ) :
			Material(MAT_REFLECT | MAT_REFRACT),
 
			F0( calcFreshnelF0(Color(n), k) ),
			n(n)
		{}
 
		SmoothMaterial( Color n, Color k ) :
			Material(MAT_SHADOW_CASTER | MAT_REFLECT),
			
			F0( calcFreshnelF0(n, k) ),
			n(0)
		{}
 
		Color Freshnel( Vector dir, Vector normal ) const {
			float cosA = fabs(normal * dir);
 
			return F0 + (Color(1) - F0) * pow(1-cosA, 5);
		}
 
 
		virtual Ray reflect( const HitRes& res ){
			Vector dir    = res.ray.dir;
			Vector normal = res.normal;
 
			Vector out = dir - normal * (normal * dir) * 2.0f;
 
			return res.createRay(out);
		}
		virtual Ray refract( const HitRes& res ){
			Vector dir    = res.ray.dir;
			Vector normal = res.normal;
 
			float ior  = n;
			float cosA = -(normal * dir);
		
			if ( cosA < 0 ){
				cosA   = -cosA;
				normal = -normal;
				
				ior = 1/n;
			}
 
			float disc = 1 - (1 - cosA*cosA)/(ior*ior);
 
			if ( disc < 0 )
				return reflect(res);
 
			Vector out  = dir/ior + normal * (cosA/ior - sqrt(disc));
			float  outC = res.ray.C /ior;
 
			return res.createRay(out, outC);
		}
};
class RoughMaterial  : public Material {
	public:
		Color kd, ks;
 
		float shiny;
 
		RoughMaterial( Color flat, float shiny = 0.5 ) :
			Material(MAT_SHADOW_CASTER),
 
			kd(flat),
			ks(flat),
			shiny(shiny)
		{}
 
		RoughMaterial( Color kd, Color ks, float shiny = 0.5 ) :
			Material(MAT_SHADOW_CASTER),
 
			kd(kd),
			ks(ks),
			shiny(shiny)
		{}
 
		virtual Color shade( Vector normal, Vector viewDir, Vector lightDir, Color inRad ) const {
			Color rad;
 
			float cosT = normal * lightDir;
			if ( cosT > 0 )
				rad = rad + inRad * kd * cosT;
 
			float cosD = normal * (viewDir + lightDir).normal();
			if ( cosD > 0 )
				rad = rad + inRad * ks * pow(cosD, shiny);
 
			return rad;
		}
};
 
 
class Matrix {
	float 
		m00, m01, m02,
		m10, m11, m12,
		m20, m21, m22;
	
	public:
		static Matrix identity(){ 
			return Matrix(
				1, 0, 0, 
				0, 1, 0, 
				0, 0, 1
			); 
		}
 
		static Matrix rotateX( float A ){
			float sinA = sin(A);
			float cosA = cos(A);
 
			return Matrix(
				cosA,  sinA, 0,
				-sinA, cosA, 0,
				0,     0,    1
			);
		}
		static Matrix rotateY( float A ){
			float sinA = sin(A);
			float cosA = cos(A);
 
			return Matrix(
				cosA,  0, -sinA,
				0,     1,     0,
				sinA,  0,  cosA
			);
		}
		static Matrix rotateZ( float A ){
			float sinA = sin(A);
			float cosA = cos(A);
 
			return Matrix(
				1,     0,    0,
				0,  cosA, sinA,
				0, -sinA, cosA
			);
		}
		
		static Matrix scale( const Vector& S ){
			return Matrix(
				S.x, 0, 0,
				0, S.y, 0,
				0, 0, S.z
			);
		}
 
		Matrix( float I = 1.0f ) :
			m00(I), m01(0), m02(0), 
			m10(0), m11(I), m12(0), 
			m20(0), m21(0), m22(I)
		{}
 
		Matrix(
			float m00, float m01, float m02,
			float m10, float m11, float m12,
			float m20, float m21, float m22
		) :
			m00(m00), m01(m01), m02(m02), 
			m10(m10), m11(m11), m12(m12), 
			m20(m20), m21(m21), m22(m22)
		{}
 
		#define MM(R, C) m##R##0 * B.m0##C + m##R##1 * B.m1##C + m##R##2 * B.m2##C 
		#define MV(R)    m##R##0 * B.x     + m##R##1 * B.y     + m##R##2 * B.z
 
		Matrix operator*( const Matrix& B ) const {
			return Matrix(
				MM(0,0),  MM(0,1),  MM(0,2),
				MM(1,0),  MM(1,1),  MM(1,2),
				MM(2,0),  MM(2,1),  MM(2,2)    
			);
		}
 
		Vector operator*( const Vector& B ) const {
			return Vector(
				MV(0),
				MV(1),
				MV(2)
			);
		}
 
};
 
 
 
 
 
struct Light {
	Vector origin;
	Vector vel;
 
	Color rad;
 
	Light( Vector origin, Vector vel, Color rad ) : 
		origin(origin),
		vel(vel),
		rad(rad)
	{}
 
 
	bool calcPastPosition( const HitRes& res, float T, float C, Vector& out ) const {
		Vector A(origin + vel * T);
		Vector V(vel);
		Vector P(res.pos);
 
		float a = V*V - C*C;
		float b = 2*(A*V - V*P);
		float c = (A-P)*(A-P);
 
		float det = b*b - 4*a*c;
 
		// No photons will reach the point until time
		if ( det < 0 )
			return false;
 
		float z0 = (-b+sqrt(det))/(2*a);
		float z1 = (-b-sqrt(det))/(2*a);
 
		// Target time
		float delta = z0 < z1 ? z0 : z1;
		
		out = A + vel * delta;
		return true;
	}
 
};
 
class SceneObj {
	Material* mat; 
 
	public:
		Vector origin;
 
		SceneObj( Material* mat, Vector origin ) :
			mat(mat),
			origin(origin)
		{}
 
		virtual HitRes tryHitObject( const Ray& ray ) const {
			return HitRes(ray);
		};
 
		virtual Material* material() const {
			return mat;
		}
};
 
 
class ScenePlane : public SceneObj {
 
	public:
		Vector normal;
		Vector up;
 
		ScenePlane( Material* mat, const Vector origin, const Vector normal, const Vector up ) :
			SceneObj(mat, origin),
 
			normal(normal.normal()),
			up(up.normal())
		{}
 
		virtual HitRes tryHitObject( const Ray& ray ) const {
			float frac = ((origin - ray.origin) *normal)/(ray.dir * normal);
 
			HitRes res(ray);
 
			if ( frac > 0 ){
				res.frac   = frac;
				res.normal = normal;
 
				res.pos = ray.origin + ray.dir * frac;
 
				res.u = (up          ) * (res.pos - origin);
				res.v = (up%normal) * (res.pos - origin);
			}
 
			return res;
		}
};
 
class SceneParaboloid : public SceneObj {
 
	public:	 
		Vector focus;
	 
		SceneParaboloid( Material* mat, const Vector origin, Vector focus ) :
			SceneObj(mat, origin),
	 
			focus(focus)
		{}
	 
		virtual HitRes tryHitObject( const Ray& ray ) const {
			Vector A(ray.origin);
			Vector B(ray.dir);
	 
			Vector F(origin +focus);
			Vector D(origin -focus);
	 
			Vector N = (F-D).normal();
	 
			float a = (B*N)*(B*N) - B*B;
			float b = ((A*N)*(B*N) - A*B - (B*N)*(D*N) + B*F) * 2;
			float c = ((A*N)*(A*N) - A*A - (A*N)*(D*N)*2 + A*F * 2 + (D*N)*(D*N) - F*F);
	 
			float det = b*b - 4*a*c;
	 
			HitRes out(ray);
	 
			if ( det > 0 ){
				float t0 = (-b + sqrt(det))/(2*a);
				float t1 = (-b - sqrt(det))/(2*a);
	 
				// Accept greater T as solution
				float T = t0 > t1 ? t0 : t1;
	 
				if ( T > 0 ){
					out.frac  = T;
					out.pos   = A + B*T;
 
					Vector fDir = (F - out.pos).normal();
 
					out.normal = (N + fDir).normal();
				}
			}
	 
			return out;
		}
};
 
 
class SceneSphere : public SceneObj {
	float radius;
 
	public:
		SceneSphere( Material* mat, const Vector origin, float radius = 1 ) :
			SceneObj(mat, origin),
			radius(radius)
		{}
 
		virtual HitRes tryHitObject( const Ray& ray ) const {
			float fracBase = ray.dir * (origin - ray.origin);
 
			Vector proj = ray.origin + ray.dir * fracBase;
			float dist  = (origin - proj).len(); 
 
			HitRes res(ray);
 
			if ( dist <= radius ){
				float fracOff = sqrt(radius*radius - dist*dist);
				float frac;
 
				if ( fracBase < fracOff )
					frac = fracBase + fracOff;
				else
					frac = fracBase - fracOff;
 
				if ( frac <= 0 )
					return res;

				res.frac   = frac;
				res.pos    = ray.origin + ray.dir * frac;
 
				res.normal = (res.pos - origin).normal();
			}
 
			return res;
		}
};
class SceneEllipse : public SceneObj {
	SceneSphere sphere;
 
	Matrix localToWorld;
	Matrix localToWorldN;
 
	Matrix worldToLocal;
 
	public:
		SceneEllipse( Material* mat, Vector origin ) :
			SceneObj(mat, origin),
			
			sphere(mat, origin, 1)
		{
			Vector S(2,1,0.5);

			float rotX = PI/4;
			float rotY = PI/4;

			Matrix scale  = Matrix::scale(S);
			Matrix scaleI = Matrix::scale( Vector(1.0f/S.x, 1.0f/S.y, 1.0f/S.z) );
 
			Matrix rotate  = Matrix::rotateX( rotX) * Matrix::rotateY( rotY);
			Matrix rotateI = Matrix::rotateY(-rotY) * Matrix::rotateX(-rotX);
 
			localToWorld  = rotate * scale;
			localToWorldN = rotate * scaleI;
 
			worldToLocal  = scaleI * rotateI;
		}
 
		virtual HitRes tryHitObject( const Ray& ray ) const {
 
			Ray local = ray;
			 local.origin = (worldToLocal * local.origin);
			 local.dir    = (worldToLocal * local.dir).normal();
 
			HitRes res = sphere.tryHitObject(local);
 
			if ( res.frac > 0 ){
				res.pos    = (localToWorld  * res.pos);
				res.normal = (localToWorldN * res.normal).normal();
 
				res.frac   = (ray.origin - res.pos).len();
			}
 
			return res;
		}
};
 
 
class SceneMover : public SceneObj {
	SceneObj* target;
	Vector    velocity;
 
	public:
		SceneMover( SceneObj* target, const Vector origin, const Vector velocity ) :
			SceneObj(NULL, origin),
 
			target(target),
			velocity(velocity)
		{}
 
		virtual HitRes tryHitObject( const Ray& ray ) const {
			Vector baseOff = velocity * ray.T - origin;       // Object moved this far already since T0
			Vector pVel    = ray.dir  * ray.C - velocity;     // Particle speed
 
			Ray helper;
			 helper.origin = ray.origin - baseOff;
			 helper.dir    = pVel.normal();
 
			HitRes res = target->tryHitObject(helper);
 
			if ( res.frac > 0 ){
				float delta = res.frac / pVel.len();    // Time passed until the particle hit
 
				HitRes out(ray);
				 out.pos    = baseOff + res.pos + velocity * delta;  // Transform back to global coordinates
				 out.normal = res.normal;                            // Translation is invariant to normals
 
				 out.frac   = (ray.origin - out.pos).len();
 
				// Final hitpoint is on the original trace line, therefore the projection was correct
				if ( fabs(ray.dir * (out.pos - ray.origin) - out.frac) > 0.0005 )
					exit(1);
 
				return out;
			}
 
			// Trace missed, no point in transforming
			return res;
		}
 
		virtual Material* material() const {
			return target->material();
		}
};

class SceneBoolean : public SceneObj {
	SceneObj* A;
	SceneObj* B;

	public:
		SceneBoolean( SceneObj* A, SceneObj* B ) :
			SceneObj(NULL, Vector()),

			A(A),
			B(B)
		{}

		static void fixResult( HitRes& res, const Ray& ray, float baseFrac ){
			// Frac must be corrected, as recursive calls cast rays 'closer' to
			// the object, therefore the discarded intersection frac must be taken
			// into account
			res.ray = ray;

			if ( res.frac > 0 )
				res.frac += baseFrac + rOff;
		}

		virtual HitRes tryHitObject( const Ray& ray ) const {
			HitRes hitA = A->tryHitObject(ray);

			if ( hitA.frac < 0 )
				return hitA;

			HitRes hitB = B->tryHitObject(ray);

			if ( hitB.frac < 0 )
				return hitA;

			HitRes miss(ray);

			// A - B
			bool insideA  = (hitA.normal * ray.dir) > 0;
			bool insideB  = (hitB.normal * ray.dir) > 0;

			bool isAFirst = hitA.frac < hitB.frac;

			// Turn B inside out
			hitB.normal = hitB.normal * -1;

			if ( hitA.frac > 0 ){
				if ( hitB.frac > 0 ){

					if ( insideA ) {
						
						if ( insideB ){
						
							if ( isAFirst ){
								// inside A, inside B = AB -> B, discard
								return miss;
							} else {
								// inside B, inside A = B -> A, inverse B
								return hitB;
							}
						} else {
							// Hit inside of A, outside of B = A/B -> 0 -> B/A
							return hitA;
						}
					} else {

						if ( insideB ){
						
							if ( isAFirst ){
								// outsideA, inside B = Skip intersection with A, trace recursively
								HitRes res = tryHitObject( hitA.createRay(ray.dir) );

								fixResult(res, ray, hitA.frac);
								return res;
							} else {
								// inside B, outside A = B -> 0 -> A, valid
								return hitA;
							}
						} else {
						
							if ( isAFirst ){
								// outside A, outside B = 0 -> A. valid
								return hitA;
							} else {
								// outside B, outside A = Skip intersections with B, trace recursively
								HitRes res = tryHitObject( hitB.createRay(ray.dir) );

								fixResult(res, ray, hitB.frac);
								return res;
							}
						}
					}

				}

				// Hitpoint on A is valid if the ray does not hit B at all
				return hitA;
			}

			// Miss
			return miss;
		}

		virtual Material* material() const {
			return A->material();
		}
};

 
const int scrW = 1280;
const int scrH = 720;
 
#define H 0.4
#define L 0.1
 
#define S 10
 
RoughMaterial colors[] = {
	RoughMaterial( Color(H,H,H), S ),
	RoughMaterial( Color(H,H,L), S ),
	RoughMaterial( Color(H,L,H), S ),
	RoughMaterial( Color(H,L,L), S ),
	RoughMaterial( Color(L,H,H), S ),
	RoughMaterial( Color(L,H,L), S ),
	RoughMaterial( Color(L,L,H), S ),
	RoughMaterial( Color(L,L,L), S )
};
 
SmoothMaterial glass( 1.5f,                   Color(0.1,0.1,0.1)         ); 
SmoothMaterial gold ( Color(0.17, 0.35, 1.5), Color(3.1, 2.7, 1.9) );
 
ScenePlane plane0( &colors[0],  Vector( 5, 0, 0 ), -Vector( 1, 0, 0 ), -Vector( 0, 0, 1 ) );
ScenePlane plane1( &colors[1], -Vector( 5, 0, 0 ),  Vector( 1, 0, 0 ),  Vector( 0, 0, 1 ) );
ScenePlane plane2( &colors[2],  Vector( 0, 5, 0 ), -Vector( 0, 1, 0 ), -Vector( 1, 0, 0 ) );
ScenePlane plane3( &colors[3], -Vector( 0, 5, 0 ),  Vector( 0, 1, 0 ),  Vector( 1, 0, 0 ) );
ScenePlane plane4( &colors[4],  Vector( 0, 0, 5 ), -Vector( 0, 0, 1 ), -Vector( 1, 0, 0 ) );
ScenePlane plane5( &colors[5], -Vector( 0, 0, 8 ),  Vector( 0, 0, 1 ),  Vector( 1, 0, 0 ) );
 
SceneParaboloid parab0( &gold, Vector( 6, 0, 0 ), Vector( -12, 0, 0 ) );

SceneEllipse ellipseShape( &glass, Vector() );
SceneMover   ellipse( &ellipseShape, Vector(2,2,2), Vector(0.1,0.1,0.1) );

Light light0( Vector(0,0,4), Vector(0,0.1,0), Color(1,1,1) );


SceneSphere sph00( &glass, Vector(0,0,-1 + 0.0f), 3.0f );
SceneSphere sph01( &glass, Vector(0,0,-1 + 0.4f), 3.2f );


SceneBoolean bool0( &sph00, &sph01 );

SceneObj* scene[] = {
	&plane1, 
	&plane2, 
	&plane3, 
	&plane4, 
	&plane5,
 
	&parab0, 
	&ellipse,

	&bool0,

	NULL
};
 
HitRes tryHitScene( const Ray& ray, int mask = 0 ){
	HitRes out(ray);
 
	for ( int index = 0; scene[index] != NULL; index++ ){
		SceneObj* obj =  scene[index];
 
		// Only trace for certain material types
		if ((obj->material()->flags & mask) != mask)
			continue;
 
		HitRes hit = obj->tryHitObject(ray);
 
		if ( hit.frac > 0 ){
			hit.obj = obj;
 
			if ( out.frac < 0 || out.frac > hit.frac )
				out = hit;
		}
	}

	return out;
}
 
int sign( float a ){
	return a > 0 ? 1 : a < 0 ? -1 : 0;
}
 
 
 
Color ambient(0);
 
Color trace( const Ray& ray, int bounce ){
	if ( --bounce < 0 )
		return ambient;
 
	HitRes res = tryHitScene(ray);
 
	if ( res.frac < 0 )
		return ambient;
 
	Material* mat = res.obj->material();
 
	Color rad;
 
	// Test if the light illuminates this point
	Vector lightPos;
 
	if ( light0.calcPastPosition(res, res.getT(), ray.C, lightPos) ){
		Vector delta = (lightPos - res.pos);
 
		Ray vRay = res.createRay( delta.normal() );
 
		HitRes occluder = tryHitScene(vRay, MAT_SHADOW_CASTER);
 
		if ( occluder.frac < 0 || occluder.frac > delta.len() ){
			Color M(1);
			 
			float u = fmod(5 + res.u/5, 1);
			float v = fmod(5 + res.v/5, 1);
 
			if ( u > 0.5 == v > 0.5 )
				M = Color(0.8);
 
			rad = mat->shade(res.normal, -ray.dir, vRay.dir, light0.rad) * M;
		}
	}
 
	if ( mat->flags & MAT_REFLECT ){
		Ray in = mat->reflect(res);
 
		rad = rad + trace(in, bounce) * mat->Freshnel(ray.dir, res.normal);
	}
 
	if ( mat->flags & MAT_REFRACT ){
		Ray in = mat->refract(res);
 
		rad = rad + trace(in, bounce) * (Color(1) - mat->Freshnel(ray.dir, res.normal));
	}

	return rad;
}
 
 
Vector camPos(-4,4,2); 
Vector camUp (0,0,1);
Vector camDir(1,-0.8,-0.2);
 
const float camFOV = 2.5f;
 
float camT = 10;
float camC = 1;
 
Ray pixelRay( int x, int y ){
	float pX = (x / (float) scrW);
	float pY = (y / (float) scrH);
 
	pX = pX - 0.5;
	pY = 0.5 - pY;
 
	Vector vF = camDir.normal();
	Vector vU = camUp.normal();
	Vector vR;
 
	vR = vF % vU;
	vU = vF % vR;
 
	float fovU = camFOV;
	float fovV = camFOV * (1280.0f/720.f);

	Vector dir = camDir + (vU * pY * fovU + vR * pX * fovV);
 
	Ray ray;
	 ray.T = camT;
	 ray.C = camC;
 
	 ray.origin = camPos;
	 ray.dir    = dir.normal();
 
	return ray;
}
 
 
Color image[scrW*scrH];
int   cY = 0;
 
void onInitialization() { 
	glViewport(0, 0, scrW, scrH);
}
 
void onDisplay() {
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 
	for ( int y = 0; y < scrH; y++ ){
		for ( int x = 0; x < scrW; x++ )
			image[y * scrW + x] = trace(pixelRay(x,y), 6);
	}
 
	glDrawPixels(scrW, scrH, GL_RGB, GL_FLOAT, image);
	glutSwapBuffers();
}
 
 
 
#define vdelta(pKey, nKey, var, delta) \
	case pKey: var += delta; break; \
	case nKey: var -= delta; break;
 
 
void onKeyboard(unsigned char key, int x, int y) {    
	switch( key ){
		case ' ':
			camT = glutGet(GLUT_ELAPSED_TIME) /1000.0f +5;
			break;
 
		case '0':    camC = 1;       break;
		case '9':    camC = 100000; break;
 
		vdelta('a', 'd', camT, 0.4)
		vdelta('e', 'r', camC, 0.1)
 
		vdelta('w', 's', camPos.z, 0.1)
 
		case 'c':
			camPos = Vector(-4,4,4); 
			camDir = Vector(1,-0.8,-0.5);
			break;
 
		case 'v':
			camPos = Vector(-4,0,0);
			camDir = Vector(1,0,0);
			break;
 
		default:
			return;
	}
 
	glutPostRedisplay();
}
 
void onKeyboardUp(unsigned char key, int x, int y) {
}
 
void onMouse(int button, int state, int x, int y) {
}
 
void onMouseMotion(int x, int y) {
}
 
void onIdle() {
}
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 
int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitWindowSize(scrW, scrH);
	glutInitWindowPosition(100, 100);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
 
	glutCreateWindow("Grafika hazi feladat");
 
	glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
 
	onInitialization();
 
	glutDisplayFunc(onDisplay);
	glutMouseFunc(onMouse);
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutKeyboardUpFunc(onKeyboardUp);
	glutMotionFunc(onMouseMotion);
 
	glutMainLoop();   
	return 0;
}

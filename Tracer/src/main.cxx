// STL includes
#include <iostream>
#include <array>
#include <thread>
#include <atomic>

// Math includes
#include "vec3.hpp"
#include "color.hpp"
#include "ray.hpp"

// Object-related includes
#include "hittable.hpp"
#include "sphere.hpp"
#include "hittableList.hpp"

// Material-related includes
//#include "material.hpp"
#include "lambertian.hpp"
#include "metal.hpp"

// Camera includes
#include "camera.hpp"

// Utility includes
#include "utility.hpp"

// Image writer includes
#include "ppmWriter.hpp"
#include "imageWriter.hpp"

// Profiling includes
#include "instrumentor.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

color rayColor(const ray& r, const HittableList& world, int depth) {
	// If we've exceeded the ray bounce limit, no more light is gathered.
	if (depth <= 0) {
		return color(0, 0, 0);
	}

	HitRecord record;

	if (world.hit(r, 0.001, infinity, record)) {
		//TODO: Investigate whether it is worth it to make the tolerance customizable.
		ray scattered;
		color attenuation;

		if (record.material->scatter(r, record, attenuation, scattered)) {
			return attenuation * rayColor(scattered, world, depth - 1);
		}

		return color(0, 0, 0);
	}

	vec3 unitDirection = unitVector(r.direction());
	//normalizing makes all the coordinates vary from [-1, 1] (inclusive)

	auto t = 0.5 * (unitDirection.y() + 1.0);
	//This is a trick to make the y value vary from [0, 1]

	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
	//This is a linear combination of the start and end colors to make a
	//smooth and linear color gradient.
}

void render(std::atomic<int> scanLinesLeft, int imageWidth, int imageHeight,
			HittableList& world, int maxRayDepth, Camera& camera, int samplesPerPixel,
			imageWriter& iw, ppmWriter& pw) {
	while (scanLinesLeft > 0) {
		std::cerr << "\rScanlines remaining: " << scanLinesLeft << " " << std::flush;
		scanLinesLeft--;

		for (int i = 0; i < imageWidth; ++i) {
			color pixelColor;
			for (int s = 0; s < samplesPerPixel; ++s) {
				auto u = (i + randomDouble()) / (imageWidth - 1);
				auto v = (j + randomDouble()) / (imageHeight - 1);

				ray r = camera.getRay(u, v);

				pixelColor += rayColor(r, world, maxRayDepth);
			}

			pixelColor.combine(samplesPerPixel);

			pw.write(pixelColor);
			iw.writeToPNGBuffer(pixelColor);
			iw.writeToJPGBuffer(pixelColor);
		}
	}
}

int main() {
	Instrumentor::get().beginSession("main");
	{
		PROFILE_FUNCTION();

		// Image
		constexpr auto aspectRatio = 16.0 / 9.0;
		constexpr int imageWidth = 1920;
		constexpr int imageHeight = static_cast<int>(imageWidth / aspectRatio);
		constexpr int samplesPerPixel = 100;
		constexpr int maxRayDepth = 50;

		// World
		HittableList world;

		auto groundMaterial = std::make_shared<Lambertian>(color(0.8, 0.8, 0.0));
		auto centerMaterial = std::make_shared<Lambertian>(color(0.7, 0.3, 0.3));
		auto leftMaterial   = std::make_shared<Metal>(color(0.8, 0.8, 0.8));
		auto rightMaterial  = std::make_shared<Metal>(color(0.8, 0.6, 0.2));

		world.add(std::make_shared<Sphere>(point3( 0.0, -100.5, -1.0), 100.0, groundMaterial));
		world.add(std::make_shared<Sphere>(point3( 0.0,    0.0, -1.0),   0.5, centerMaterial));
		world.add(std::make_shared<Sphere>(point3(-1.0,    0.0, -1.0),   0.5, leftMaterial));
		world.add(std::make_shared<Sphere>(point3( 1.0,    0.0, -1.0),   0.5, rightMaterial));

		// Camera
		Camera camera;

		//Initialize file writers
		ppmWriter pw(imageWidth, imageHeight, false);
		imageWriter iw(imageWidth, imageHeight);

		// Thread-related initializations
		constexpr int numThreads = std::thread::hardware_concurrency();
		auto threadPool = std::array<std::thread, numThreads>();
		//This will be modified by multiple threads so it needs to be thread-safe
		std::atomic<int> scanLinesLeft = imageHeight - 1;

		// Kick off each thread with the render() task
		for (int i = 0; i < numThreads; ++i) {
			threadPool[i] = std::thread(render, scanLinesLeft, imageWidth, imageHeight,
										world, maxRayDepth, camera, samplesPerPixel,
										iw, pw);
		}

		std::cerr << "\n";

		// Wait for all threads to finish their tasks
		for (auto& t: threadPool) {
			t.join();
		}

		// Write image file to disk
		if (iw.writePNG() != 0) {
			std::cout << "PNG Image generated successfully.\n";
		} else {
			std::cout << "An error occurred while generating the PNG image.\n";
		}

		if (iw.writeJPG() != 0) {
			std::cout << "JPG Image generated successfully.\n";
		} else {
			std::cout << "An error occurred while generating the JPG image.\n";
		}

		std::cerr << "Done.\n";
	}
	Instrumentor::get().endSession();
}

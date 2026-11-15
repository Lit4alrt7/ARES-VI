#include "World/MarsTerrainGenerator.h"

#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AMarsTerrainGenerator::AMarsTerrainGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(TerrainMesh);
	TerrainMesh->bUseAsyncCooking = true;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RockMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Boulders = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Boulders"));
	Boulders->SetupAttachment(TerrainMesh);
	if (RockMesh.Succeeded())
	{
		Boulders->SetStaticMesh(RockMesh.Object);
	}
	Boulders->SetCollisionProfileName(TEXT("BlockAll"));

	TerrainMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/MI_MarsRegolith.MI_MarsRegolith")));
	BoulderMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/MI_MarsRock.MI_MarsRock")));
}

void AMarsTerrainGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bGenerateInEditor)
	{
		Regenerate();
	}
}

void AMarsTerrainGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (!bGenerated)
	{
		Regenerate();
	}
}

void AMarsTerrainGenerator::Regenerate()
{
	BuildCraters();
	BuildMesh();
	ScatterBoulders();
	bGenerated = true;
}

void AMarsTerrainGenerator::BuildCraters()
{
	FRandomStream Rng(Seed);
	Craters.Reset();
	const float Half = SizeCm * 0.5f;
	for (int32 i = 0; i < NumCraters; ++i)
	{
		FMarsCrater Crater;
		// Power-law size distribution: many small craters, few large ones.
		const float SizeRoll = FMath::Pow(Rng.FRand(), 2.5f);
		Crater.Radius = FMath::Lerp(1500.0f, 22000.0f, SizeRoll);
		Crater.Depth = Crater.Radius * 2.0f * Rng.FRandRange(0.12f, 0.2f);

		// Keep craters off the base pad.
		for (int32 Attempt = 0; Attempt < 10; ++Attempt)
		{
			Crater.Center = FVector2D(Rng.FRandRange(-Half, Half), Rng.FRandRange(-Half, Half));
			if (Crater.Center.Size() > FlatPadRadiusCm + Crater.Radius * 1.6f)
			{
				break;
			}
		}
		Craters.Add(Crater);
	}
}

float AMarsTerrainGenerator::SampleHeightLocal(float X, float Y) const
{
	const float Offset = static_cast<float>(Seed % 997) * 13.37f;

	// fBm rolling plains.
	float Height = 0.0f;
	float Amplitude = HillAmplitudeCm;
	float Frequency = 1.0f / 60000.0f;
	for (int32 Octave = 0; Octave < 4; ++Octave)
	{
		Height += FMath::PerlinNoise2D(FVector2D(X * Frequency + Offset, Y * Frequency - Offset)) * Amplitude;
		Amplitude *= 0.45f;
		Frequency *= 2.1f;
	}

	// Wind-aligned dune ridges (asymmetric: gentle stoss slope, steep lee slope).
	const float WindRad = FMath::DegreesToRadians(WindDirectionDegrees);
	const FVector2D WindDir(FMath::Cos(WindRad), FMath::Sin(WindRad));
	const float Along = FVector2D::DotProduct(FVector2D(X, Y), WindDir);
	const float Across = FVector2D::DotProduct(FVector2D(X, Y), FVector2D(-WindDir.Y, WindDir.X));
	const float Wobble = FMath::PerlinNoise2D(FVector2D(Across / 20000.0f + Offset, 0.5f)) * 3000.0f;
	const float Phase = FMath::Frac((Along + Wobble) / 9000.0f);
	const float Ridge = Phase < 0.8f ? Phase / 0.8f : (1.0f - Phase) / 0.2f;
	const float DuneMask = FMath::Clamp(FMath::PerlinNoise2D(FVector2D(X / 50000.0f - Offset, Y / 50000.0f)) * 1.5f + 0.4f, 0.0f, 1.0f);
	Height += Ridge * DuneAmplitudeCm * DuneMask;

	// Simple craters: parabolic bowl + raised rim decaying outward (ejecta blanket).
	for (const FMarsCrater& Crater : Craters)
	{
		const float D = FVector2D::Distance(FVector2D(X, Y), Crater.Center) / Crater.Radius;
		if (D < 1.0f)
		{
			Height += Crater.Depth * (D * D - 1.0f) + Crater.Depth * 0.25f;
		}
		else if (D < 2.5f)
		{
			Height += Crater.Depth * 0.25f * FMath::Exp(-(D - 1.0f) * 3.0f);
		}
	}

	// Ancient dry riverbed: a meandering channel with a flat sandy floor and terraced banks.
	{
		const float CenterY = 26000.0f + 16000.0f * FMath::Sin(X / 52000.0f + 0.7f) + 5000.0f * FMath::Sin(X / 17000.0f + 1.3f);
		const float Dist = FMath::Abs(Y - CenterY);
		const float HalfWidth = 5200.0f + 1500.0f * FMath::Sin(X / 23000.0f);
		const float Depth = 950.0f;
		if (Dist < HalfWidth * 2.2f)
		{
			const float T = Dist / (HalfWidth * 2.2f);
			const float Bank = 1.0f - FMath::SmoothStep(0.35f, 1.0f, T);           // 1 on the floor, 0 outside
			const float Terrace = 0.15f * FMath::Sin(T * PI * 3.0f) * Bank;
			Height = FMath::Lerp(Height, Height * 0.2f, Bank) - Depth * (Bank + Terrace);
		}
	}

	// Flatten the base pad with a smooth falloff.
	const float PadDist = FVector2D(X, Y).Size();
	const float PadBlend = FMath::SmoothStep(FlatPadRadiusCm, FlatPadRadiusCm * 2.0f, PadDist);
	return Height * PadBlend;
}

float AMarsTerrainGenerator::GetHeightAtWorld(float WorldX, float WorldY) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(FVector(WorldX, WorldY, 0.0f));
	return GetActorLocation().Z + SampleHeightLocal(Local.X, Local.Y);
}

void AMarsTerrainGenerator::BuildMesh()
{
	const int32 Cells = FMath::Clamp(FMath::RoundToInt(SizeCm / CellSizeCm), 2, 400);
	const float Step = SizeCm / Cells;
	const float Half = SizeCm * 0.5f;

	// Height grid.
	TArray<float> Heights;
	Heights.SetNumUninitialized((Cells + 1) * (Cells + 1));
	for (int32 y = 0; y <= Cells; ++y)
	{
		for (int32 x = 0; x <= Cells; ++x)
		{
			Heights[y * (Cells + 1) + x] = SampleHeightLocal(-Half + x * Step, -Half + y * Step);
		}
	}

	// Faceted: unique vertices per triangle so each face is flat-shaded (PS1 look).
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const int32 NumTris = Cells * Cells * 2;
	Vertices.Reserve(NumTris * 3);
	Triangles.Reserve(NumTris * 3);
	Normals.Reserve(NumTris * 3);
	UVs.Reserve(NumTris * 3);
	Colors.Reserve(NumTris * 3);

	// Smooth shading: per-vertex normals from the height field (central differences) and colours from
	// slope + low-frequency noise. (Per-face normals and random per-face colours read as a checkerboard.)
	auto H = [&](int32 x, int32 y)
	{
		return Heights[FMath::Clamp(y, 0, Cells) * (Cells + 1) + FMath::Clamp(x, 0, Cells)];
	};
	auto P = [&](int32 x, int32 y)
	{
		return FVector(-Half + x * Step, -Half + y * Step, H(x, y));
	};
	auto VNormal = [&](int32 x, int32 y)
	{
		return FVector(H(x - 1, y) - H(x + 1, y), H(x, y - 1) - H(x, y + 1), 2.0f * Step).GetSafeNormal();
	};
	auto VColor = [&](int32 x, int32 y)
	{
		const FVector N = VNormal(x, y);
		const float Slope = 1.0f - N.Z;
		const float Var = FMath::PerlinNoise2D(FVector2D(x * 0.07f, y * 0.07f)) * 0.06f + FMath::PerlinNoise2D(FVector2D(x * 0.31f, y * 0.31f)) * 0.02f;
		const FLinearColor Ochre(0.62f + Var, 0.30f + Var * 0.6f, 0.16f + Var * 0.3f);
		const FLinearColor Basalt(0.25f, 0.16f, 0.12f);
		return FLinearColor::LerpUsingHSV(Ochre, Basalt, FMath::Clamp(Slope * 3.0f, 0.0f, 1.0f));
	};
	auto AddTri = [&](int32 ax, int32 ay, int32 bx, int32 by, int32 cx, int32 cy)
	{
		const int32 Base = Vertices.Num();
		const int32 Xs[3] = { ax, bx, cx };
		const int32 Ys[3] = { ay, by, cy };
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector V = P(Xs[k], Ys[k]);
			Vertices.Add(V);
			Normals.Add(VNormal(Xs[k], Ys[k]));
			UVs.Add(FVector2D(V.X, V.Y) / 1000.0f);
			Colors.Add(VColor(Xs[k], Ys[k]));
		}
		Triangles.Add(Base);
		Triangles.Add(Base + 1);
		Triangles.Add(Base + 2);
	};

	for (int32 y = 0; y < Cells; ++y)
	{
		for (int32 x = 0; x < Cells; ++x)
		{
			// Consistent diagonal (alternating ones produce a visible diamond / checker pattern).
			AddTri(x, y, x + 1, y + 1, x + 1, y);
			AddTri(x, y, x, y + 1, x + 1, y + 1);
		}
	}

	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
	if (UMaterialInterface* Mat = TerrainMaterial.LoadSynchronous())
	{
		TerrainMesh->SetMaterial(0, Mat);
	}
}

void AMarsTerrainGenerator::ScatterBoulders()
{
	Boulders->ClearInstances();
	FRandomStream Rng(Seed + 7);
	const float Half = SizeCm * 0.5f;

	for (int32 i = 0; i < NumBoulders; ++i)
	{
		FVector2D XY(Rng.FRandRange(-Half, Half), Rng.FRandRange(-Half, Half));
		// Boulders cluster around crater rims (ejecta).
		if (Craters.Num() > 0 && Rng.FRand() < 0.6f)
		{
			const FMarsCrater& Crater = Craters[Rng.RandRange(0, Craters.Num() - 1)];
			const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
			const float R = Crater.Radius * Rng.FRandRange(1.0f, 1.8f);
			XY = Crater.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R;
		}
		if (XY.Size() < FlatPadRadiusCm * 0.8f)
		{
			continue;
		}

		const float Scale = FMath::Pow(Rng.FRand(), 3.0f) * 4.0f + 0.3f;
		const FVector Location(XY.X, XY.Y, SampleHeightLocal(XY.X, XY.Y) + Scale * 20.0f);
		const FRotator Rotation(Rng.FRandRange(-30.0f, 30.0f), Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-30.0f, 30.0f));
		const FVector Scale3D(Scale * Rng.FRandRange(0.7f, 1.3f), Scale * Rng.FRandRange(0.7f, 1.3f), Scale * Rng.FRandRange(0.4f, 0.9f));
		Boulders->AddInstance(FTransform(Rotation, Location, Scale3D));
	}

	if (UMaterialInterface* Mat = BoulderMaterial.LoadSynchronous())
	{
		Boulders->SetMaterial(0, Mat);
	}
}

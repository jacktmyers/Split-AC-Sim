# Simulation Method

| Value | Method |
|-------|--------|
| 0 | Weakly Compressible SPH (WCSPH) |
| 1 | Predictive-Corrective Incompressible SPH (PCISPH) |
| 2 | Position Based Fluids (PBF) |
| 3 | Implicit Incompressible SPH (IISPH) |
| 4 | Divergence-Free SPH (DFSPH) |
| 5 | Projective Fluids (PF) |
| 6 | Implicit Compressible SPH (ICSPH) |

---

## 0 — Weakly Compressible SPH (WCSPH)
The classic SPH approach. Uses a stiff equation of state (Tait equation) to relate density to pressure, enforcing near-incompressibility by making compression very energetically costly. Simple to implement but requires very small timesteps to stay stable, making it slow for large simulations.

## 1 — Predictive-Corrective Incompressible SPH (PCISPH)
An improvement over WCSPH. It uses a prediction-correction loop — it predicts particle positions, checks how much density error results, then corrects pressures iteratively until density stays within a threshold. Allows much larger timesteps than WCSPH while still being relatively straightforward.

## 2 — Position Based Fluids (PBF)
Comes from the Position Based Dynamics (PBD) family. Instead of solving forces/pressures, it directly corrects particle *positions* to satisfy a density constraint each step. Very stable and GPU-friendly, which is why it's popular in real-time and game applications (e.g. Nvidia FleX). Less physically accurate but extremely fast.

Does not simulate compression behavior of gasses, however at a large scale these effects are negligible.

## 3 — Implicit Incompressible SPH (IISPH)
Formulates pressure as an implicit linear system and solves it with a relaxed Jacobi iteration. Much more accurate and stable than WCSPH/PCISPH, converges faster per timestep, and handles larger timesteps well. A solid general-purpose choice for offline simulation.

## 4 — Divergence-Free SPH (DFSPH)
Extends IISPH with a second solver pass that enforces *divergence-free velocity* in addition to constant density. This two-pass approach (divergence solve + density solve) dramatically reduces artificial compressibility and allows even larger timesteps. Currently one of the state-of-the-art methods for accuracy/performance balance.

## 5 — Projective Fluids (PF)
Adapts the Projective Dynamics framework to SPH fluids. Uses a global linear system with a fixed system matrix (pre-factored once), then does local constraint projections each iteration. This makes iterations very cheap. It's fast in practice but can be less accurate than DFSPH, and the global solve can be costly to set up for dynamic scenes.

## 6 — Implicit Compressible SPH (ICSPH)
Targets *compressible* flows (gases, shocks, high-speed phenomena) rather than liquids. Solves the full compressible Euler/Navier-Stokes equations implicitly, allowing larger timesteps than explicit compressible SPH while capturing compressibility effects that methods 0–5 deliberately suppress.

---

## Quick Comparison

| | Incompressible | Timestep | Speed | Use Case |
|---|---|---|---|---|
| WCSPH | ✗ (weak) | Tiny | Slow | Simple liquids |
| PCISPH | ✓ | Medium | Moderate | General liquids |
| PBF | ✓ | Large | Fast | Realtime/games |
| IISPH | ✓ | Large | Good | Offline sim |
| DFSPH | ✓✓ | Largest | Best | High-quality offline |
| PF | ✓ | Large | Fast | Large-scale offline |
| ICSPH | ✗ (fully) | Medium | Moderate | Gases/compressible |

# Time Step

## CFL
* This is an adaptive time stepping method

# Particle Radius
The particleRadius parameter is a fundamental physics parameter that affects multiple aspects of the simulation, not just the visual appearance.
How particleRadius Affects Physics
1. Particle Mass
From the source code, particle mass is calculated as:
$$\text{Mass} = \text{volume} \times \text{density0}$$
Where volume depends on particle diameter ($2 \times \text{particleRadius}$):
- 3D: $volume \propto (2 \times \text{radius})^3$
- 2D: $volume \propto (2 \times \text{radius})^2$
Changing particleRadius directly changes particle mass, affecting momentum and pressure calculations.
2. Support Radius (Kernel Size)
The support radius for all SPH kernels is fixed at 4× particleRadius:
m_supportRadius = 4.0 * m_particleRadius;
This means:
- Particle spacing determines neighbor search radius
- Kernel smoothing length automatically adjusts
- Changing particleRadius changes all SPH interpolation properties
3. Time Step Size (CFL Condition)
From the CFL computation:
$$\Delta t \propto \text{particle diameter} / \sqrt{\text{max velocity}}$$
Smaller particleRadius → smaller time steps → longer simulation times.
4. Boundary Handling
Boundary distance calculations use particleRadius to ensure particles maintain proper distance from walls, preventing artificial pressure forces.
5. Viscosity and Other Forces
All non-pressure forces (viscosity, surface tension, vorticity) use the support radius, which depends on particleRadius.
Practical Impact
If you change particleRadius without adjusting other parameters:
- Smaller radius → much more particles needed (O(n³) cost increase), shorter time steps
- Larger radius → fewer particles, faster simulation, but less detail and potential clumping
Recommendation
Keep particleRadius consistent with:
- Your domain size (aim for 10-20 particles across characteristic dimensions)
- The desired simulation resolution
- Performance constraints
The default value of 0.025 represents a reasonable balance between accuracy and performance.
Full technical details: particleRadius Physics in SPlisHSPlasH (https://github.com/InteractiveComputerGraphics/SPlisHSPlasH/blob/master/doc/file_format.md#configuration)
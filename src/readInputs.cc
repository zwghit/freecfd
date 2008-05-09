#include "inputs.h"

void read_inputs(InputFile &input) {

	input.register_section("equations");
	input.section["equations"].register_string("set");
	// defaults
	input.section["equations"].strings["set"]="Euler";
	input.read_section("equations");

	input.register_section("timeMarching");
	input.section["timeMarching"].register_string("type");
	input.section["timeMarching"].register_double("step");
	input.section["timeMarching"].register_double("CFL");
	input.section["timeMarching"].register_int("numberOfSteps");
	input.section["timeMarching"].register_int("outFreq");
	// defaults
	input.section["timeMarching"].strings["type"]="CFL";
	input.section["timeMarching"].doubles["CFL"]=1.;
	input.section["timeMarching"].ints["numberOfSteps"]=100;
	input.section["timeMarching"].ints["outFreq"]=10;
	input.read_section("timeMarching");

	input.register_section("initialConditions");
	input.section["initialConditions"].register_numberedSubsection("region");
	input.section["initialConditions"].numberedSubsections["region"].register_string("type");
	input.section["initialConditions"].numberedSubsections["region"].register_Vec3D("box_1");
	input.section["initialConditions"].numberedSubsections["region"].register_Vec3D("box_2");
	input.section["initialConditions"].numberedSubsections["region"].register_double("rho");
	input.section["initialConditions"].numberedSubsections["region"].register_Vec3D("center");
	input.section["initialConditions"].numberedSubsections["region"].register_double("radius");
	input.section["initialConditions"].numberedSubsections["region"].register_Vec3D("v");
	input.section["initialConditions"].numberedSubsections["region"].register_double("p");
	input.read_section("initialConditions");

	input.register_section("boundaryConditions");
	input.section["boundaryConditions"].register_numberedSubsection("BC");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_string("type");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_string("region");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_Vec3D("box_1");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_Vec3D("box_2");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_double("rho");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_double("p");
	input.section["boundaryConditions"].numberedSubsections["BC"].register_Vec3D("v");
	input.read_section("boundaryConditions");

	input.register_section("fluidProperties");
	input.section["fluidProperties"].register_double("gamma");
	input.section["fluidProperties"].register_subsection("viscosity");
	input.section["fluidProperties"].subsections["viscosity"].register_string("type");
	input.section["fluidProperties"].subsections["viscosity"].register_double("value");
	// defaults
	input.section["fluidProperties"].subsections["viscosity"].strings["type"]="fixed";
	input.read_section("fluidProperties");

	input.register_section("numericalOptions");
	input.section["numericalOptions"].register_string("order");
	input.section["numericalOptions"].register_string("limiter");
	input.section["numericalOptions"].register_double("sharpeningFactor");
	// defaults
	input.section["numericalOptions"].strings["order"]="first";
	input.section["numericalOptions"].strings["limiter"]="superbee";
	input.section["numericalOptions"].doubles["sharpeningFactor"]=0.;
	input.read_section("numericalOptions");

	input.register_section("solutionFormat");
	input.section["solutionFormat"].register_string("input");
	input.section["solutionFormat"].register_string("output");
	// defaults
	input.section["solutionFormat"].strings["input"]="vtk";
	input.section["solutionFormat"].strings["output"]="vtk";
	input.read_section("solutionFormat");

}

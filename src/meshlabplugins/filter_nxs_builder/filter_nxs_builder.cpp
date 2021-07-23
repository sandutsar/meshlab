/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005-2021                                           \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/

#include "filter_nxs_builder.h"

#include <thread>
#include <QTextStream>
#include <QTemporaryDir>
#include <nxsbuild/nexusbuilder.h>
#include <nxsbuild/meshstream.h>
#include <nxsbuild/plyloader.h>
#include <nxsbuild/kdtree.h>

#include <nxsbuild/nexusbuilder.h>
#include <common/traversal.h>
#include <nxsedit/extractor.h>

/**
 * @brief
 * Constructor usually performs only two simple tasks of filling the two lists
 *  - typeList: with all the possible id of the filtering actions
 *  - actionList with the corresponding actions.
 * If you want to add icons to your filtering actions you can do here by construction the QActions accordingly
 */
NxsBuilderPlugin::NxsBuilderPlugin()
{ 
	typeList = {
		FP_NXS_BUILDER,
		FP_NXS_COMPRESS
	};

	for(const ActionIDType& tt : typeList)
		actionList.push_back(new QAction(filterName(tt), this));
}

QString NxsBuilderPlugin::pluginName() const
{
	return "NxsBuilder";
}

QString NxsBuilderPlugin::vendor() const
{
	return "CNR-ISTI-VCLab";
}

/**
 * @brief ST() must return the very short string describing each filtering action
 * (this string is used also to define the menu entry)
 * @param filterId: the id of the filter
 * @return the name of the filter
 */
QString NxsBuilderPlugin::filterName(ActionIDType filterId) const
{
	switch(filterId) {
	case FP_NXS_BUILDER :
		return "NXS Build";
	case FP_NXS_COMPRESS :
		return "NXS Compress";
	default :
		assert(0);
		return "";
	}
}


/**
 * @brief // Info() must return the longer string describing each filtering action
 * (this string is used in the About plugin dialog)
 * @param filterId: the id of the filter
 * @return an info string of the filter
 */
QString NxsBuilderPlugin::filterInfo(ActionIDType filterId) const
{
	switch(filterId) {
	case FP_NXS_BUILDER :
		return "Create a nxs file starting from a obj, ply or stl.";
	default :
		assert(0);
		return "Unknown Filter";
	}
}

/**
 * @brief The FilterClass describes in which generic class of filters it fits.
 * This choice affect the submenu in which each filter will be placed
 * More than a single class can be chosen.
 * @param a: the action of the filter
 * @return the class od the filter
 */
NxsBuilderPlugin::FilterClass NxsBuilderPlugin::getClass(const QAction *a) const
{
	switch(ID(a)) {
	case FP_NXS_BUILDER :
	case FP_NXS_COMPRESS:
		return FilterPlugin::Other;
	default :
		assert(0);
		return FilterPlugin::Generic;
	}
}

/**
 * @brief FilterSamplePlugin::filterArity
 * @return
 */
FilterPlugin::FilterArity NxsBuilderPlugin::filterArity(const QAction*) const
{
	return NONE;
}

/**
 * @brief FilterSamplePlugin::getPreConditions
 * @return
 */
int NxsBuilderPlugin::getPreConditions(const QAction*) const
{
	return MeshModel::MM_NONE;
}

/**
 * @brief FilterSamplePlugin::postCondition
 * @return
 */
int NxsBuilderPlugin::postCondition(const QAction*) const
{
	return MeshModel::MM_NONE;
}

/**
 * @brief This function returns a list of parameters needed by each filter.
 * For each parameter you need to define,
 * - the name of the parameter,
 * - the default value
 * - the string shown in the dialog
 * - a possibly long string describing the meaning of that parameter (shown as a popup help in the dialog)
 * @param action
 * @param m
 */
RichParameterList NxsBuilderPlugin::initParameterList(const QAction *action, const MeshModel &)
{
	RichParameterList params;
	switch(ID(action)) {
	case FP_NXS_BUILDER :
		params.addParam(RichOpenFile("input_file", "", {"*.ply", "*.obj", "*.stl", "*.tsp"}, "", ""));
		params.addParam(RichSaveFile("output_file", "", "*.nxs", "", ""));
		params.addParam(RichInt("node_faces", 1<<15, "Node faces", "Number of faces per patch"));
		params.addParam(RichInt("top_node_faces", 4096, "Top node faces", "Number of triangles in the top node"));
		params.addParam(RichInt("tex_quality", 100, "Texture quality [0-100]", "jpg texture quality"));
		params.addParam(RichInt("ram", 2000, "Ram buffer", "Max ram used (in MegaBytes)", true));
		params.addParam(RichInt("skiplevels", 0, "Skip levels", "Decimation skipped for n levels"));
		params.addParam(RichPoint3f("origin", Point3m(0,0,0), "Origin", "new origin for the model"));
		params.addParam(RichBool("center", false, "Center", "Set origin in the bounding box center", true));
		params.addParam(RichBool("pow_2_textures", false, "Pow 2 textures", "Create textures to be power of 2", true));
		params.addParam(RichBool("deepzoom", false, "Deepzoom", "Save each node and texture to a separated file", true));
		params.addParam(RichDynamicFloat("adaptive", 0.333, 0, 1, "Adaptive", "Split nodes adaptively"));
		break;
	case FP_NXS_COMPRESS:
		params.addParam(RichOpenFile("input_file", "", {"*.nxs"}, "", ""));
		params.addParam(RichSaveFile("out_file", "", "*.nxz", "", ""));
		params.addParam(RichFloat("nxz_vertex_quantization", 0.0, "NXZ Vertex quantization", "absolute side of quantization grid (uses quantization factor, instead)", false, "NXZ parameters"));
		params.addParam(RichInt("vertex_bits", 0, "Vertex bits", "number of bits in vertex coordinates when compressing (uses quantization factor, instead)", false, "NXZ parameters"));
		params.addParam(RichFloat("quantization_factor", 0.1, "Quantization factor", "Quantization as a factor of error", false, "NXZ parameters"));
		params.addParam(RichInt("luma_bits", 6, "Luma bits", "Quantization of luma channel", true, "NXZ parameters"));
		params.addParam(RichInt("chroma_bits", 6, "Chroma bits", "Quantization of chroma channel", true, "NXZ parameters"));
		params.addParam(RichInt("alpha_bits", 5, "Alpha bits", "Quantization of alpha channel", true, "NXZ parameters"));
		params.addParam(RichInt("normal_bits", 10, "Normal bits", "Quantization of normals", true, "NXZ parameters"));
		params.addParam(RichFloat("textures_precision", 0.25, "Textures precision", "Quantization of textures, precision in pixels per unit", true, "NXZ parameters"));
	default :
		assert(0);
	}
	return params;
}

/**
 * @brief The Real Core Function doing the actual mesh processing.
 * @param action
 * @param md: an object containing all the meshes and rasters of MeshLab
 * @param par: the set of parameters of each filter, with the values set by the user
 * @param cb: callback object to tell MeshLab the percentage of execution of the filter
 * @return true if the filter has been applied correctly, false otherwise
 */
std::map<std::string, QVariant> NxsBuilderPlugin::applyFilter(
		const QAction* action,
		const RichParameterList& par,
		MeshDocument&,
		unsigned int& /*postConditionMask*/,
		vcg::CallBackPos*)
{
	switch(ID(action)) {
	case FP_NXS_BUILDER :
		nxsBuild(par);
		break;
	case FP_NXS_COMPRESS:
		break;
	default :
		wrongActionCalled(action);
	}
	return std::map<std::string, QVariant>();
}

void NxsBuilderPlugin::nxsBuild(const RichParameterList& params)
{
	QString inputFile = params.getOpenFileName("input_file");
	QString outputFile = params.getSaveFileName("output_file");

	//parameters:
	int node_size = params.getInt("node_faces");
	int top_node_size = params.getInt("top_node_faces");
	float vertex_quantization = 0;
	int tex_quality = params.getInt("tex_quality");
	float scaling = 0.5;
	int skiplevels = params.getInt("skiplevels");
	int ram_buffer = params.getInt("ram");
	int n_threads = std::thread::hardware_concurrency() / 2;
	if (n_threads == 0)
		n_threads = 1;

	vcg::Point3d origin = vcg::Point3d::Construct(params.getPoint3m("origin"));
	bool center = params.getBool("center");

	bool useOrigTex = false;
	bool create_pow_two_tex = params.getBool("pow_2_textures");
	bool deepzoom = params.getBool("deepzoom");
	QVariant adaptive = params.getDynamicFloat("adaptive");

	Stream* stream = nullptr;
	KDTree* tree = nullptr;

	bool point_cloud = false;
	bool normals = false;
	bool no_normals = false;
	bool colors = false;
	bool no_colors = false;
	bool no_texcoords = false;

	try {
		quint64 max_memory = (1<<20)*(uint64_t)ram_buffer/4; //hack 4 is actually an estimate...

		//autodetect point cloud ply
		if(inputFile.endsWith(".ply")) {
			PlyLoader autodetect(inputFile);
			if(autodetect.nTriangles() == 0)
				point_cloud = true;
		}

		std::string input = "mesh";

		if (point_cloud) {
			input = "pointcloud";
			stream = new StreamCloud("cache_stream");
		}
		else
			stream = new StreamSoup("cache_stream");

		stream->setVertexQuantization(vertex_quantization);
		stream->setMaxMemory(max_memory);
		if(center) {
			vcg::Box3d box = stream->getBox(QStringList(inputFile));
			stream->origin = box.Center();
		}
		else {
			stream->origin = origin;
		}

		vcg::Point3d &o = stream->origin;
		if(o[0] != 0.0 || o[1] != 0.0 || o[2] != 0.0) {
			int lastPoint = outputFile.lastIndexOf(".");
			QString ref = outputFile.left(lastPoint) + ".js";
			QFile file(ref);
			if(!file.open(QFile::ReadWrite)) {
				throw MLException("Could not save reference file: " + ref);
			}
			QTextStream stream(&file);
			stream.setRealNumberPrecision(12);
			stream << "{ \"origin\": [" << o[0] << ", " << o[1] << ", " << o[2] << "] }\n";
		}
		//TODO: actually the stream will store textures or normals or colors even if not needed
		stream->load(QStringList(inputFile), ""); //second parameter should be the mtl file....

		bool has_colors = stream->hasColors();
		bool has_normals = stream->hasNormals();
		bool has_textures = stream->hasTextures();

		quint32 components = 0;
		if(!point_cloud) components |= NexusBuilder::FACES;

		if((!no_normals && (!point_cloud || has_normals)) || normals) {
			components |= NexusBuilder::NORMALS;
		}
		if((has_colors  && !no_colors ) || colors ) {
			components |= NexusBuilder::COLORS;
		}
		if(has_textures && !no_texcoords) {
			components |= NexusBuilder::TEXTURES;
		}

		//WORKAROUND to save loading textures not needed
		if(!(components & NexusBuilder::TEXTURES)) {
			stream->textures.clear();
		}

		NexusBuilder builder(components);
		builder.skipSimplifyLevels = skiplevels;
		builder.setMaxMemory(max_memory);
		builder.n_threads = n_threads;
		builder.setScaling(scaling);
		builder.useNodeTex = !useOrigTex;
		builder.createPowTwoTex = create_pow_two_tex;
		if(deepzoom)
			builder.header.signature.flags |= nx::Signature::Flags::DEEPZOOM;
		builder.tex_quality = tex_quality;
		bool success = builder.initAtlas(stream->textures);
		if(!success) {
			throw MLException("Fail when initializing atlas");
		}

		if(point_cloud)
			tree = new KDTreeCloud("cache_tree", adaptive.toFloat());
		else
			tree = new KDTreeSoup("cache_tree", adaptive.toFloat());

		tree->setMaxMemory((1<<20)*(uint64_t)ram_buffer/2);
		KDTreeSoup *treesoup = dynamic_cast<KDTreeSoup *>(tree);
		if(treesoup)
			treesoup->setTrianglesPerBlock(node_size);

		KDTreeCloud *treecloud = dynamic_cast<KDTreeCloud *>(tree);
		if(treecloud)
			treecloud->setTrianglesPerBlock(node_size);

		builder.create(tree, stream, top_node_size);
		builder.save(outputFile);
	}
	catch(const MLException& error) {
		if(tree)   delete tree;
		if(stream) delete stream;
		throw error;
	}
	catch(QString error) {
		if(tree)   delete tree;
		if(stream) delete stream;
		throw MLException("Fatal error: " + error);

	}
	catch(const char *error) {
		if(tree)   delete tree;
		if(stream) delete stream;
		throw MLException("Fatal error: " + QString(error));
	}

	if(tree)   delete tree;
	if(stream) delete stream;
}

MESHLAB_PLUGIN_NAME_EXPORTER(NxsBuilderPlugin)

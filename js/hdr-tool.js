/*
*   Alex Rodriguez
*   @jxarco 
*/

// HDRE CLASS

/*
*   Alex Rodriguez
*   @jxarco 
*/

// hdre.js 

//main namespace
(function(global){

	var GL = _GL;

    if(!GL)
    throw( "HDRTool.js needs litegl.js to work" );

	/**
	 * Main namespace
	 * @namespace HDRE
	 */
	
	var FLO2BYTE = 4;
	var BYTE2BITS = 8;

	var U_BYTE		= 1;
	var HALF_FLOAT	= 2;
	var FLOAT		= 3;
	var U_BYTE_RGBE	= 4;

	var ARRAY_TYPES = {
		1: Uint8Array,
		2: Uint16Array,
		3: Float32Array,
		4: Uint8Array
	}

	var HDRE = global.HDRE = {

		version: 3.0,	// v1.5 adds spherical harmonics coeffs for the skybox
						// v2.0 adds byte padding for C++ uses				
						// v2.5 allows mip levels to be smaller than 8x8
						// v2.75 RGB format supported
						// v3.0 HDREImage and HDREBuilder
		maxFileSize: 60e6 // bytes
	};

	HDRE.DEFAULT 	= 0;
	HDRE.EXR 		= 1;
	HDRE.RADIANCE 	= 2;

	HDRE.CUBE_MAP_POSITIVE_X = 0;
	HDRE.CUBE_MAP_POSITIVE_Y = 1;
	HDRE.CUBE_MAP_POSITIVE_Z = 2;
	HDRE.CUBE_MAP_NEGATIVE_X = 3;
	HDRE.CUBE_MAP_NEGATIVE_Y = 4;
	HDRE.CUBE_MAP_NEGATIVE_Z = 5;
	
	HDRE.setup = function(o)
	{
		o = o || {};
		if(HDRE.configuration)
			throw("setup already called");
		HDRE.configuration = o;
	}
	
	/** HEADER STRUCTURE (256 bytes)
	 * Header signature ("HDRE" in ASCII)	   4 bytes
	 * Format Version						   4 bytes
	 * Width									2 bytes
	 * Height								   2 bytes
	 * Max file size							4 bytes
	 * Number of channels					   1 byte
	 * Bits per channel						 1 byte
	 * Header size								1 byte
	 * Max luminance							4 byte
	 * Flags									1 byte
	 */
	
	/**
	* This class stores all the HDRE data
	* @class HDREImage
	*/

	function HDREImage(header, data, o) {

		if(this.constructor !== HDREImage)
		throw("You must use new to create a HDREImage");

		this._ctor(header, data);

		if(o)
		this.configure(o);
	}

	HDRE.HDREImage = HDREImage;

	HDREImage.prototype._ctor = function(h, data) {

		//could I build hdre without header?
		h = h || {};

		// file info
		this.version = h["version"];

		// dimensions
		this.width = h["width"];
		this.height = h["height"];

		// channel info
		this.n_channels = h["nChannels"];
		this.bits_channel = h["bpChannel"];
		
		// image info
		this.data = data;
		this.type = ARRAY_TYPES[h["type"]];
		this.max_irradiance = h["maxIrradiance"];
		this.shs = h["shs"];
		this.size = h["size"];

		// store h just in case
		this.header = h;
	}
	
	HDREImage.prototype.configure = function(o) {

		o = o || {};

		this.is_rgbe = o.rgbe !== undefined ? o.rgbe : false;

		for(var k in o)
		this[k] = o[k];

		if(this.bits_channel === 32)
			this.type = Float32Array;
		else
			this.type = Uint8Array;

	}

	HDREImage.prototype.flipY = function() {

	}

	HDREImage.prototype.toTexture = function() {
		
		var _envs = this.data;
		if(!_envs)
			return false;

		// Get base enviroment texture
		var tex_type = GL.FLOAT;
		var data = _envs[0].data;
		var w = this.width;

		if(this.type === Uint16Array) // HALF FLOAT
			tex_type = GL.HALF_FLOAT_OES;
		else if(this.type === Uint8Array) 
			tex_type = GL.UNSIGNED_BYTE;

		
		var flip_Y_sides = false;

		// "true" for using old environments
        // (not standard flipY)
		if(this.version < 3.0)
		flip_Y_sides = true;

		if(flip_Y_sides)
		{
			var tmp = data[2];
			data[2] = data[3];
			data[3] = tmp;
		}

		var options = {
			format: this.n_channels === 4 ? gl.RGBA : gl.RGB,
			type: tex_type,
			minFilter: gl.LINEAR_MIPMAP_LINEAR,
			texture_type: GL.TEXTURE_CUBE_MAP,
		};

		GL.Texture.disable_deprecated = true;

		var tex = new GL.Texture( w, w, options );
		tex.mipmap_data = {};

		// Generate mipmaps
		tex.bind(0);

		var num_mipmaps = Math.log(w) / Math.log(2);

		// Upload prefilter mipmaps
		for(var i = 0; i <= num_mipmaps; i++)
		{
			var level_info = _envs[i];
			var levelsize = Math.pow(2,num_mipmaps - i);

			if(level_info)
			{
				var pixels = level_info.data;

				if(flip_Y_sides && i > 0)
				{
					var tmp = pixels[2];
					pixels[2] = pixels[3];
					pixels[3] = tmp;
				}

				for(var f = 0; f < 6; ++f)
				{
					if(!flip_Y_sides && i == 0)
					{
						GL.Texture.flipYData( pixels[f], w, w, this.n_channels);
					}

					tex.uploadData( pixels[f], { cubemap_face: f, mipmap_level: i}, true );
				}
				tex.mipmap_data[i] = pixels;
			}
			else
			{
				var zero = new Float32Array(levelsize * levelsize * this.n_channels);
				for(var f = 0; f < 6; ++f)
					tex.uploadData( zero, { cubemap_face: f, mipmap_level: i}, true );
			}
		}

		GL.Texture.disable_deprecated = false;

		// Store the texture 
		tex.has_mipmaps = true;
		tex.data = null;
		tex.is_rgbe = this.is_rgbe;

		return tex;
	}

	/**
	* This class creates HDRE from different sources
	* @class HDREBuilder
	*/

	function HDREBuilder(o) {

		if(this.constructor !== HDREBuilder)
		throw("You must use new to create a HDREBuilder");

		this._ctor();

		if(o)
		this.configure(o);
	}
	
	HDRE.HDREBuilder = HDREBuilder;

	HDREBuilder.prototype._ctor = function() {

		this.flip_Y_sides = true;
		this.pool = {};
		this.last_id = 0;
	}

	HDREBuilder.prototype.configure = function(o) {

		o = o || {};
	}

	HDREBuilder.prototype.createImage = function(data, size) {

		if(!data)
		throw("[error] cannot create HDRE image");

		var texture = null;
		var image = new HDREImage();

		//create gpu texture from file
		if(data.constructor !== GL.Texture)
			texture = this.createTexture(data, size);
		else
			texture = data;
				
		image.configure({
			version: 3.0,
			width: texture.width,
			height: texture.height,
			n_channels: texture.format === GL.RGB ? 3 : 4,
			bits_channel: texture.type === GL.FLOAT ? 32 : 8,
			texture: texture,
			id: this.last_id
		})

		this.pool[this.last_id++] = image;
		console.log(this.pool);

		return image;
	}

	HDREBuilder.prototype.fromFile = function(buffer, options) {

		var image = HDRE.parse(buffer, options);

		this.pool[this.last_id++] = image;
		// console.log(this.pool);

		if(options.callback)
			options.callback(image);

		return image;
	}

	HDREBuilder.prototype.fromHDR = function(filename, buffer, size) {

		var data, ext = filename.split('.').pop();

		switch (ext) {
			case "hdr":
				data = this._parseRadiance( buffer );
					break;
		
			case "exr":
				data = this._parseEXR( buffer );
					break;

			default:
				throw("cannot parse hdr file");
		}

		//add HDRE image to the pool
		return this.createImage(data, size);
	}

	HDREBuilder.prototype.fromTexture = function(texture) {

		this.filter(texture, {
			oncomplete: (function(result) {
				
				this.createImage(result);
				
			}).bind(this)
		});
	}

	/**
    * Create a texture based in data received as input 
    * @method CreateTexture
    * @param {Object} data 
    * @param {Number} cubemap_size
    */
   	HDREBuilder.prototype.createTexture = function( data, cubemap_size, options )
	{
		if(!data)
		throw( "No data to get texture" );

		options = options || {};		

		var width = data.width,
			height = data.height;

		var is_cubemap = ( width/4 === height/3 && GL.isPowerOfTwo(width) ) ? true : false;

		var channels = data.numChannels;
		var pixelData = data.rgba;
		var pixelFormat = channels === 4 ? gl.RGBA : gl.RGB; // EXR and HDR files are written in 4 

		if(!width || !height)
		throw( 'No width or height to generate Texture' );

		if(!pixelData)
		throw( 'No data to generate Texture' );

		var texture = null;

		var params = {
			format: pixelFormat,
			type: gl.FLOAT,
			pixel_data: pixelData
		};

		GL.Texture.disable_deprecated = true;

		// 1 image cross cubemap
		if(is_cubemap)
		{
			var square_length = pixelData.length / 12;
			var faces = parseFaces(square_length, width, height, pixelData);

			width /= 4;
			height /= 3;

			params.texture_type = GL.TEXTURE_CUBE_MAP;
			params.pixel_data = faces;

			texture = new GL.Texture( width, height, params);

			var temp = texture.clone();
			var shader = new GL.Shader(Shader.SCREEN_VERTEX_SHADER, HDRE.CopyCubemap_Shader_Fragment);
			
			//save state
			var current_fbo = gl.getParameter( gl.FRAMEBUFFER_BINDING );
			var viewport = gl.getViewport();
			var fb = gl.createFramebuffer();
			gl.bindFramebuffer( gl.FRAMEBUFFER, fb );
			gl.viewport(0,0, width, height);

			var mesh = Mesh.getScreenQuad();
			
			// Bind original texture
			texture.bind(0);
			mesh.bindBuffers( shader );
			shader.bind();

			var rot_matrix = GL.temp_mat3;
			var cams = GL.Texture.cubemap_camera_parameters;

			for(var i = 0; i < 6; i++)
			{
				gl.framebufferTexture2D( gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_CUBE_MAP_POSITIVE_X + i, temp.handler, 0);
				var face_info = cams[i];

				mat3.identity( rot_matrix );
				rot_matrix.set( face_info.right, 0 );
				rot_matrix.set( face_info.up, 3 );
				rot_matrix.set( face_info.dir, 6 );
				shader._setUniform( "u_rotation", rot_matrix );
				shader._setUniform( "u_flip", true );
				gl.drawArrays( gl.TRIANGLES, 0, 6 );
			}

			mesh.unbindBuffers( shader );
			//restore previous state
			gl.setViewport(viewport); //restore viewport
			gl.bindFramebuffer( gl.FRAMEBUFFER, current_fbo ); //restore fbo
			gl.bindTexture(temp.texture_type, null); //disable

			temp.is_cubemap = is_cubemap;
		}

		// basic texture or sphere map
		else 
		{
			texture = new GL.Texture( width, height, params);
		}
			
		// texture properties
		texture.wrapS = gl.CLAMP_TO_EDGE;
		texture.wrapT = gl.CLAMP_TO_EDGE;
		texture.magFilter = gl.LINEAR;
		texture.minFilter = gl.LINEAR_MIPMAP_LINEAR;

		if(is_cubemap)
			return temp;

		if(!options.discard_spheremap)
			gl.textures["tmp_spheremap"] = texture;

		
		var result = this.toCubemap( texture, cubemap_size );
		GL.Texture.disable_deprecated = false;

		return result;
	}

	/**
	 * Converts spheremap or panoramic map to a cubemap texture 
	 * @method ToCubemap
	 * @param {Texture} tex
	 * @param {Number} cubemap_size
	 */
	HDREBuilder.prototype.toCubemap = function( tex, cubemap_size )
	{
		var size = cubemap_size || this.CUBE_MAP_SIZE;
		if(!size)
		throw( "CUBEMAP size not defined" );

		//save state
		var current_fbo = gl.getParameter( gl.FRAMEBUFFER_BINDING );
		var viewport = gl.getViewport();
		var fb = gl.createFramebuffer();
		gl.bindFramebuffer( gl.FRAMEBUFFER, fb );
		gl.viewport(0,0, size, size);

		var shader_type = (tex.width == tex.height * 2) ? HDRE.LatLong_Shader_Fragment : HDRE.Spheremap_Shader_Fragment;
		var shader = new GL.Shader(Shader.SCREEN_VERTEX_SHADER, shader_type);

		if(!shader)
			throw( "No shader" );

		// Bind original texture
		tex.bind(0);
		var mesh = Mesh.getScreenQuad();
		mesh.bindBuffers( shader );
		shader.bind();

		var cubemap_texture = new GL.Texture( size, size, { format: tex.format, texture_type: GL.TEXTURE_CUBE_MAP, type: gl.FLOAT, minFilter: GL.LINEAR_MIPMAP_LINEAR } );
		var rot_matrix = GL.temp_mat3;
		var cams = GL.Texture.cubemap_camera_parameters;

		for(var i = 0; i < 6; i++)
		{
			gl.framebufferTexture2D( gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_texture.handler, 0);
			// var face_info = cams[i];
			// mat3.identity( rot_matrix );
			// rot_matrix.set( face_info.right, 0 );
			// rot_matrix.set( face_info.up, 3 );
			// rot_matrix.set( face_info.dir, 6 );
			// shader._setUniform( "u_rotation", rot_matrix );
			shader._setUniform( "u_face", i );
			gl.drawArrays( gl.TRIANGLES, 0, 6 );
		}

		mesh.unbindBuffers( shader );

		//restore previous state
		gl.setViewport(viewport); //restore viewport
		gl.bindFramebuffer( gl.FRAMEBUFFER, current_fbo ); //restore fbo
		gl.bindTexture(cubemap_texture.texture_type, null); //disable

		var pixels = cubemap_texture.getCubemapPixels();

		if(this.flip_Y_sides)
		{
			var tmp = pixels[2];
			pixels[2] = pixels[3];
			pixels[3] = tmp;
		}

		for(var f = 0; f < 6; ++f)
		{
			// if(this.flip_Y_sides)
			// 	GL.Texture.flipYData(pixels[f], size, size, tex.format === GL.RGBA ? 4 : 3);

			cubemap_texture.uploadData( pixels[f], { cubemap_face: f } );
		}
			

		return cubemap_texture;
	}

	/**
	 * Blurs each of the level of a given environment texture
	 * @method Filter
	 * @param {Texture} texture
	 * @param {Object} options
	 */
	HDREBuilder.prototype.filter = function(texture, options) {

        var options = options || {};

		if(!texture)
		throw("no texture to filter");

        var shader = new Shader(HDRE.Filter_Shader_Vertex, HDRE.Filter_Shader_Fragment);
		var mipCount = 5;

		//Reset Builder steps
		this.LOAD_STEPS = 0;
		this.CURRENT_STEP = 0;

		//Clean previous mipmap data
		texture.mipmap_data = {
			0: texture.getCubemapPixels()
		};

		// compute necessary steps
		for( var i = 1; i <= mipCount; ++i )
		{
			var faces = 6;
			var blocks = Math.min(texture.width / Math.pow( 2, i ), 8);
			this.LOAD_STEPS += faces * blocks;
		}

		GL.Texture.disable_deprecated = true;

		for( let mip = 1; mip <= mipCount; mip++ )
		{
			this._blur( texture, mip, mipCount, shader, (function(result) {

				var pixels = result.getCubemapPixels();

				//data always comes in rgba when reading pixels from textures
				if(texture.format == GL.RGB)
				{
					for(var f = 0; f < 6; ++f)
						pixels[f] = _removeAlphaChannel(pixels[f]);
				}

				texture.mipmap_data[mip] = pixels;

				/*if(this.flip_Y_sides)
				{
					var tmp = pixels[2];
					pixels[2] = pixels[3];
					pixels[3] = tmp;
				}*/

				for(var f = 0; f < 6; ++f)
					texture.uploadData( pixels[f], { cubemap_face: f, mipmap_level: mip}, true );

				if(this.CURRENT_STEP == this.LOAD_STEPS)
				{
					texture.data = null;

					// format is stored different when reading hdre files!! 
					if(options.image_id)
					this.pool[options.image_id].data = texture.mipmap_data;

					if(options.oncomplete)
						options.oncomplete(texture);

					GL.Texture.disable_deprecated = false;
				}

			}).bind(this));
		}
	}

	/**
    * Blurs a texture calling different draws from data
    * @method blur
    * @param {Texture} input
    * @param {Number} level
    * @param {Shader||String} shader
    */
   	HDREBuilder.prototype._blur = function(input, level, mipCount, shader, oncomplete)
	{
		var data = this._getDrawData(input, level, mipCount);
	
		if(!data)
		throw('no data to blur');
		
		// var channels = 

		var options = {
			format: input.format, //gl.RGBA,
			type: GL.FLOAT,
			minFilter: gl.LINEAR_MIPMAP_LINEAR,
			texture_type: GL.TEXTURE_CUBE_MAP
		};

		var result = new GL.Texture( data.size, data.size, options );
		var current_draw = 0;

		//save state
		var current_fbo = gl.getParameter( gl.FRAMEBUFFER_BINDING );
		var viewport = gl.getViewport();

		var fb = gl.createFramebuffer();
		var mesh = GL.Mesh.getScreenQuad();

		var inner_blur = function() {

			let drawInfo = data.draws[current_draw];
			drawInfo.uniforms["u_mipCount"] = mipCount;
			drawInfo.uniforms["u_emsize"] = input.width;

			if(!shader)
				throw( "No shader" );
	
			// bind blur fb each time 
			gl.bindFramebuffer( gl.FRAMEBUFFER, fb );

			input.bind(0);
			shader.bind();
			mesh.bindBuffers( shader );

			gl.framebufferTexture2D( gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_CUBE_MAP_POSITIVE_X + drawInfo.face, result.handler, 0);
			gl.viewport( drawInfo.viewport[0], drawInfo.viewport[1], drawInfo.viewport[2], drawInfo.viewport[3] );
			
			shader.uniforms( drawInfo.uniforms );
			gl.drawArrays( gl.TRIANGLES, 0, 6 );

			mesh.unbindBuffers( shader );
			input.unbind();

			//restore previous state each draw
			gl.setViewport(viewport); //restore viewport
			gl.bindFramebuffer( gl.FRAMEBUFFER, current_fbo ); //restore fbo
			gl.bindTexture(result.texture_type, null);
		}

		var that = this;

		var interval = setInterval( function() {

			inner_blur();
			current_draw++;

			that.CURRENT_STEP++;
			
			if(current_draw == data.draws.length)
			{
				clearInterval(interval);

				if(oncomplete)
					oncomplete( result );
			}
	   }, 100 );
   }

	/**
   * Gets info to blur in later pass
   * @method getDrawData
   * @param {Texture} input
   * @param {Number} level
   * @param {Shader} shader
   */
  	HDREBuilder.prototype._getDrawData = function(input, level, mipCount)
   	{
		var blocks = 8;

		var size = input.height; // by default
		size /= Math.pow(2, level);

		// Recompute number of blocks
		blocks = Math.min(blocks, size);

		var totalLevels = mipCount;
		var roughness = (level+1) / (totalLevels + 1);

		var deferredInfo = {};

		var cams = GL.Texture.cubemap_camera_parameters;
		var cubemap_cameras = [];
		var draws = [];

		for( let c in cams ) {

			let face_info = cams[c];
			let rot_matrix = mat3.create();
			mat3.identity( rot_matrix );
			rot_matrix.set( face_info.right, 0 );
			rot_matrix.set( face_info.up, 3 );
			rot_matrix.set( face_info.dir, 6 );
			cubemap_cameras.push( rot_matrix );
		}

		cubemap_cameras = GL.linearizeArray( cubemap_cameras );
		
		for(var i = 0; i < 6; i++)
		{
			var face_info = cams[i];

			let rot_matrix = mat3.create();
			mat3.identity( rot_matrix );
			rot_matrix.set( face_info.right, 0 );
			rot_matrix.set( face_info.up, 3 );
			rot_matrix.set( face_info.dir, 6 );

			for( var j = 0; j < blocks; j++ )
			{
				let uniforms = {
						'u_rotation': rot_matrix,
						'u_blocks': blocks,
						'u_mipCount': mipCount,
						'u_roughness': roughness,
						'u_ioffset': j * (1/blocks),
						'u_cameras': cubemap_cameras,
						'u_color_texture': 0
					};

				let blockSize = size/blocks;

				draws.push({
					uniforms: uniforms, 
					viewport: [j * blockSize, 0, blockSize, size],
					face: i
				});
			}
		}

		deferredInfo['blocks'] = blocks;
		deferredInfo['draws'] = draws;
		deferredInfo['size'] = size;
		deferredInfo['roughness'] = roughness;
		deferredInfo['level'] = level;

		return deferredInfo;
	}

	/**
    * Parse the input data and get all the EXR info 
    * @method _parseEXR
    * @param {ArrayBuffer} buffer 
    */
   	HDREBuilder.prototype._parseEXR = function( buffer )
	{
		if(!Module.EXRLoader)
			console.error('[cannot parse exr file] this function needs tinyexr.js to work');

		function parseNullTerminatedString( buffer, offset ) {

			var uintBuffer = new Uint8Array( buffer );
			var endOffset = 0;
		
			while ( uintBuffer[ offset.value + endOffset ] != 0 ) 
				endOffset += 1;
		
			var stringValue = new TextDecoder().decode(
			new Uint8Array( buffer ).slice( offset.value, offset.value + endOffset )
			);
		
			offset.value += (endOffset + 1);
		
			return stringValue;
		
		}
		
		function parseFixedLengthString( buffer, offset, size ) {
		
			var stringValue = new TextDecoder().decode(
			new Uint8Array( buffer ).slice( offset.value, offset.value + size )
			);
		
			offset.value += size;
		
			return stringValue;
		
		}

		
		function parseUint32( buffer, offset ) {
		
			var Uint32 = new DataView( buffer.slice( offset.value, offset.value + 4 ) ).getUint32( 0, true );
			offset.value += 4;
			return Uint32;
		}
		
		function parseUint8( buffer, offset ) {
		
			var Uint8 = new DataView( buffer.slice( offset.value, offset.value + 1 ) ).getUint8( 0, true );
			offset.value += 1;
			return Uint8;
		}
		
		function parseFloat32( buffer, offset ) {
		
			var float = new DataView( buffer.slice( offset.value, offset.value + 4 ) ).getFloat32( 0, true );
			offset.value += 4;
			return float;
		}
		
		function parseUint16( buffer, offset ) {
		
			var Uint16 = new DataView( buffer.slice( offset.value, offset.value + 2 ) ).getUint16( 0, true );
			offset.value += 2;
			return Uint16;
		}
		
		function parseChlist( buffer, offset, size ) {
		
			var startOffset = offset.value;
			var channels = [];
		
			while ( offset.value < ( startOffset + size - 1 ) ) {
		
				var name = parseNullTerminatedString( buffer, offset );
				var pixelType = parseUint32( buffer, offset ); // TODO: Cast this to UINT, HALF or FLOAT
				var pLinear = parseUint8( buffer, offset );
				offset.value += 3; // reserved, three chars
				var xSampling = parseUint32( buffer, offset );
				var ySampling = parseUint32( buffer, offset );
			
				channels.push( {
					name: name,
					pixelType: pixelType,
					pLinear: pLinear,
					xSampling: xSampling,
					ySampling: ySampling
				} );
			}
		
			offset.value += 1;
		
			return channels;
		}
		
		function parseChromaticities( buffer, offset ) {
		
			var redX = parseFloat32( buffer, offset );
			var redY = parseFloat32( buffer, offset );
			var greenX = parseFloat32( buffer, offset );
			var greenY = parseFloat32( buffer, offset );
			var blueX = parseFloat32( buffer, offset );
			var blueY = parseFloat32( buffer, offset );
			var whiteX = parseFloat32( buffer, offset );
			var whiteY = parseFloat32( buffer, offset );
		
			return { redX: redX, redY: redY, greenX, greenY, blueX, blueY, whiteX, whiteY };
		}
		
		function parseCompression( buffer, offset ) {
		
			var compressionCodes = [
			'NO_COMPRESSION',
			'RLE_COMPRESSION',
			'ZIPS_COMPRESSION',
			'ZIP_COMPRESSION',
			'PIZ_COMPRESSION'
			];
		
			var compression = parseUint8( buffer, offset );
		
			return compressionCodes[ compression ];
		
		}
		
		function parseBox2i( buffer, offset ) {
		
			var xMin = parseUint32( buffer, offset );
			var yMin = parseUint32( buffer, offset );
			var xMax = parseUint32( buffer, offset );
			var yMax = parseUint32( buffer, offset );
		
			return { xMin: xMin, yMin: yMin, xMax: xMax, yMax: yMax };
		}
		
		function parseLineOrder( buffer, offset ) {
		
			var lineOrders = [
			'INCREASING_Y'
			];
		
			var lineOrder = parseUint8( buffer, offset );
		
			return lineOrders[ lineOrder ];
		}
		
		function parseV2f( buffer, offset ) {
		
			var x = parseFloat32( buffer, offset );
			var y = parseFloat32( buffer, offset );
		
			return [ x, y ];
		}
		
		function parseValue( buffer, offset, type, size ) {
		
			if ( type == 'string' || type == 'iccProfile' ) {
				return parseFixedLengthString( buffer, offset, size );
			} else if ( type == 'chlist' ) {
				return parseChlist( buffer, offset, size );
			} else if ( type == 'chromaticities' ) {
				return parseChromaticities( buffer, offset );
			} else if ( type == 'compression' ) {
				return parseCompression( buffer, offset );
			} else if ( type == 'box2i' ) {
				return parseBox2i( buffer, offset );
			} else if ( type == 'lineOrder' ) {
				return parseLineOrder( buffer, offset );
			} else if ( type == 'float' ) {
				return parseFloat32( buffer, offset );
			} else if ( type == 'v2f' ) {
				return parseV2f( buffer, offset );
			} 
		}

		var EXRHeader = {};

		var magic = new DataView( buffer ).getUint32( 0, true );
		var versionByteZero = new DataView( buffer ).getUint8( 4, true );
		var fullMask = new DataView( buffer ).getUint8( 5, true );

		// Start parsing header
		var offset = { value: 8 };
		var keepReading = true;

		// clone buffer
		buffer = buffer.slice(0);

		while( keepReading )
		{
			var attributeName = parseNullTerminatedString( buffer, offset );

			if ( attributeName == 0 )
				keepReading = false;
			else
			{
				var attributeType = parseNullTerminatedString( buffer, offset );
				var attributeSize = parseUint32( buffer, offset );
				var attributeValue = parseValue( buffer, offset, attributeType, attributeSize );
				EXRHeader[ attributeName ] = attributeValue;
			}
		}

		if (EXRHeader.compression === undefined)
		throw "EXR compression is undefined";

		var width = EXRHeader.dataWindow.xMax - EXRHeader.dataWindow.xMin + 1;
		var height = EXRHeader.dataWindow.yMax - EXRHeader.dataWindow.yMin + 1;
		var numChannels = EXRHeader.channels.length;

		var byteArray;

		//if (EXRHeader.compression === 'ZIP_COMPRESSION' || EXRHeader.compression == 'NO_COMPRESSION') {

			// get all content from the exr
			try {
				var data = new Uint8Array(buffer);
				var exr = new Module.EXRLoader(data);

				if(exr.ok())
					byteArray = exr.getBytes();
				else 
					throw( "Error getting bytes from EXR file" );

			} catch (error) {
				console.error(error);
			}

		/* }
		else
		{
			console.error('Cannot decompress unsupported compression');
			return; 
		}*/

		var data = {
			header: EXRHeader,
			width: width,
			height: height,
			rgba: byteArray,
			numChannels: numChannels
		};

		return data;
	}

	/**
    * Parse the input data and get all the HDR (radiance file) info 
    * @method parseRadiance
    * @param {ArrayBuffer} buffer 
    */
	HDREBuilder.prototype._parseRadiance = function( buffer )
	{
		if(!parseHdr)
			console.error('[cannot parse hdr file] this function needs hdr-parser.js to work');
		
		var img = parseHdr(buffer);

		var data = {
			header: null,
			width: img.shape[0],
			height: img.shape[1],
			rgba: img.data,
			numChannels: img.data.length/(img.shape[0]*img.shape[1])
		};

		return data;
	}
	   
	/* 
		General HDRE Functions: Write, load, parse
	*/

	/**
	* Write and download an HDRE
	* @method write
	* @param {Object} mips_data - [lvl0: { w, h, pixeldata: [faces] }, lvl1: ...]
	* @param {Number} width
	* @param {Number} height
	* @param {Object} options
	*/
	HDRE.write = function( mips_data, width, height, options )
	{
		options = options || {};

		var array_type = Float32Array;
		
		if(options.type && options.type.BYTES_PER_ELEMENT)
			array_type = options.type;

		var RGBE = options.rgbe !== undefined ? options.rgbe : false;

		/*
		*   Create header
		*/

		// get total pixels
		var size = 0;
		for(var i = 0; i < mips_data.length; i++)
			size += mips_data[i].width * mips_data[i].height;

		// File format information
		var numFaces = 6;
		var numChannels = options.channels || 4;
		var headerSize = 256; // Bytes (256 in v2.0)
		var contentSize = size * numFaces * numChannels * array_type.BYTES_PER_ELEMENT; // Bytes
		var fileSize = headerSize + contentSize; // Bytes
		var bpChannel = array_type.BYTES_PER_ELEMENT * BYTE2BITS; // Bits

		var contentBuffer = new ArrayBuffer(fileSize);
		var view = new DataView(contentBuffer);

		var LE = true;// little endian

		// Signature: "HDRE" in ASCII
		// 72, 68, 82, 69

		// Set 4 bytes of the signature
		view.setUint8(0, 72);
		view.setUint8(1, 68);
		view.setUint8(2, 82);
		view.setUint8(3, 69);
		
		// Set 4 bytes of version
		view.setFloat32(4, this.version, LE);

		// Set 2 bytes of width, height
		view.setUint16(8, width, LE);
		view.setUint16(10, height, LE);
		// Set max file size
		view.setFloat32(12, this.maxFileSize, LE);

		// Set rest of the bytes
		view.setUint16(16, numChannels, LE); // Number of channels
		view.setUint16(18, bpChannel, LE); // Bits per channel
		view.setUint16(20, headerSize, LE); // max header size
		view.setUint16(22, LE ? 1 : 0, LE); // endian encoding

		/*
		*   Create data
		*/
		
		var data = new array_type(size * numFaces * numChannels);
		var offset = 0;

		for(var i = 0; i < mips_data.length; i++)
		{
			let _env = mips_data[i],
				w = _env.width,
				h = _env.height,
				s = w * h * numChannels;

			var suboff = 0;

			for(var f = 0; f < numFaces; f++) {
				var subdata = _env.pixelData[f];

				// remove alpha channel to save storage
				if(numChannels === 3)
					subdata = _removeAlphaChannel( subdata );

				data.set( subdata, offset + suboff);
				suboff += subdata.length;
			}

			// Apply offset
			offset += (s * numFaces);
		}

		// set max value for luminance
		view.setFloat32(24, _getMax( data ), LE); 

		var type = FLOAT;
		if( array_type === Uint8Array)
			type = U_BYTE;
		if( array_type === Uint16Array)
			type = HALF_FLOAT;

		if(RGBE)
			type = U_BYTE_RGBE;
			
		// set write array type 
		view.setUint16(28, type, LE); 

		// SH COEFFS
		if(options.sh) {
		
			var SH = options.sh;

			view.setUint16(30, 1, LE);
			view.setFloat32(32, SH.length / 3, LE); // number of coeffs
			var pos = 36;
			for(var i = 0; i < SH.length; i++) {
				view.setFloat32(pos, SH[i], LE); 
				pos += 4;
			}
		}
		else
			view.setUint16(30, 0, LE);

		/*
		*  END OF HEADER
		*/

		offset = headerSize;

		// Set data into the content buffer
		for(var i = 0; i < data.length; i++)
		{
			if(type == U_BYTE || type == U_BYTE_RGBE) {
				view.setUint8(offset, data[i]);
			}else if(type == HALF_FLOAT) {
				view.setUint16(offset, data[i], true);
			}else {
				view.setFloat32(offset, data[i], true);
			}

			offset += array_type.BYTES_PER_ELEMENT;
		}

		// Return the ArrayBuffer with the content created
		return contentBuffer;
	}

	function _getMax(data) {
		return data.reduce((max, p) => p > max ? p : max, data[0]);
	}

	function _removeAlphaChannel(data) {
		var tmp_data = new Float32Array(data.length * 0.75);
		var index = k = 0;
		data.forEach( function(a, b){  
			if(index < 3) {
				tmp_data[k++] = a;  
				index++;
			} else {
				index = 0;
			}
		});
		return tmp_data;
	}

	window.getMaxOfArray = _getMax;

	/**
	* Read file
	* @method read
	* @param {String} file 
	*/
	HDRE.load = function( url, callback )
	{
		var xhr = new XMLHttpRequest();
		xhr.responseType = "arraybuffer";
		xhr.open( "GET", url, true );
		xhr.onload = (e) => {
		if(e.target.status == 404)
			return;
		var data = HDRE.parse(e.target.response);
		if(callback)
			callback(data);
		}
		xhr.send();
	}
	
	//legacy
	HDRE.read = function( url, callback )
	{
	   console.warn("Legacy function, use HDRE.load instead of HDRE.read");
	   return HDRE.load( url, callback );
	}

	/**
	* Parse the input data and create texture
	* @method parse
	* @param {ArrayBuffer} buffer 
	* @param {Function} options (oncomplete, onprogress, filename, ...)
	*/
	HDRE.parse = function( buffer, options )
	{
		if(!buffer)
			throw( "No data buffer" );

		var options = options || {};
		var fileSizeInBytes = buffer.byteLength;
		var LE = true;

		/*
		*   Read header
		*/

		// Read signature
		var sg = parseSignature( buffer, 0 );

		// Read version
		var v = parseFloat32(buffer, 4, LE);

		// Get 2 bytes of width, height
		var w = parseUint16(buffer, 8, LE);
		var h = parseUint16(buffer, 10, LE);
		// Get max file size in bytes
		var m = parseFloat(parseFloat32(buffer, 12, LE));

		// Set rest of the bytes
		var c = parseUint16(buffer, 16, LE);
		var b = parseUint16(buffer, 18, LE);
		var s = parseUint16(buffer, 20, LE);
		var isLE = parseUint16(buffer, 22, LE);

		var i = parseFloat(parseFloat32(buffer, 24, LE));
		var a = parseUint16(buffer, 28, LE);

		var shs = null;
		var hasSH = parseUint16(buffer, 30, LE);

		if(hasSH) {
			var Ncoeffs = parseFloat32(buffer, 32, LE) * 3;
			shs = [];
			var pos = 36;

			for(var i = 0; i < Ncoeffs; i++)  {
				shs.push( parseFloat32(buffer, pos, LE) );
				pos += 4;
			}
		}

		var header = {
			version: v,
			signature: sg,
			type: a,
			width: w,
			height: h,
			nChannels: c,
			bpChannel: b,
			maxIrradiance: i,
			shs: shs,
			encoding: isLE,
			size: fileSizeInBytes
		};

		// console.table(header);
		window.parsedFile = HDRE.last_parsed_file = { buffer: buffer, header: header };
		
		if(v < 2 || v > 1e3){ // bad encoding
			console.error('old version, please update the HDRE');
			return false;
		}
		if(fileSizeInBytes > m){
			console.error('file too big');
			return false;
		}


		/*
		*   BEGIN READING DATA
		*/

		var dataBuffer = buffer.slice(s);
		var array_type = ARRAY_TYPES[header.type];

		var dataSize = dataBuffer.byteLength / 4;
		var data = new array_type(dataSize);
		var view = new DataView(dataBuffer);
		
		var pos = 0;

		for(var i = 0 ; i < dataSize; i++)
		{
			data[i] = view.getFloat32(pos, LE);
			pos += 4;
		}

		var numChannels = c;

		var ems = [],
			precomputed = [];

		var offset = 0;
		var originalWidth = w;

		for(var i = 0; i < 6; i++)
		{
			var mip_level = i + 1;
			var offsetEnd = w * w * numChannels * 6;
			ems.push( data.slice(offset, offset + offsetEnd) );
			offset += offsetEnd;
		
			if(v > 2.0)
				w = originalWidth / Math.pow(2, mip_level);
			else
				w = Math.max(8, originalWidth / Math.pow(2, mip_level));
		}

		/*
			Get bytes
		*/
		
		// care about new sizes (mip map chain)
		w = header.width;

		for(var i = 0; i < 6; i++)
		{
			var bytes = ems[i];
		
			// Reorder faces
			var faces = [];
			var bPerFace = bytes.length / 6;

			var offset = 0;

			for(var j = 0; j < 6; j++)
			{
				faces[j] = new array_type(bPerFace);

				var subdata = bytes.slice(offset, offset + (numChannels * w * w));
				faces[j].set(subdata);

				offset += (numChannels * w * w);
			}

			precomputed.push( {
				data: faces,
				width: w
			});

			// resize next textures
			var mip_level = i + 1;
			
			if(v > 2.0)
				w = originalWidth / Math.pow(2, mip_level);
			else
				w = Math.max(8, originalWidth / Math.pow(2, mip_level));

			if(options.onprogress)
				options.onprogress( i );
		}

		var image = new HDREImage(header, precomputed, options);
		return image;
	}

	// Shader Code

	//Read environment mips
	HDRE.read_cubemap_fs = '\
		vec3 readPrefilteredCube(samplerCube cube_texture, float roughness, vec3 R) {\n\
			float f = roughness * 5.0;\n\
			vec3 color = textureCubeLodEXT(cube_texture, R, f).rgb;\n\
			return color;\n\
		}\n\
	';

	//Show SHs
	HDRE.irradiance_shs_fs = '\
		varying vec3 v_normal;\n\
		uniform vec3 u_sh_coeffs[9];\n\
		\n\
		const float Pi = 3.141592654;\n\
		const float CosineA0 = Pi;\n\
		const float CosineA1 = (2.0 * Pi) / 3.0;\n\
		const float CosineA2 = Pi * 0.25;\n\
		\n\
		struct SH9 { float c[9]; };\n\
		struct SH9Color { vec3 c[9]; };\n\
		\n\
		void SHCosineLobe(in vec3 dir, out SH9 sh)\n\
		{\n\
			// Band 0\n\
			sh.c[0] = 0.282095 * CosineA0;\n\
			\n\
			// Band 1\n\
			sh.c[1] = 0.488603 * dir.y * CosineA1;\n\
			sh.c[2] = 0.488603 * dir.z * CosineA1;\n\
			sh.c[3] = 0.488603 * dir.x * CosineA1;\n\
			\n\
			sh.c[4] = 1.092548 * dir.x * dir.y * CosineA2;\n\
			sh.c[5] = 1.092548 * dir.y * dir.z * CosineA2;\n\
			sh.c[6] = 0.315392 * (3.0 * dir.z * dir.z - 1.0) * CosineA2;\n\
			sh.c[7] = 1.092548 * dir.x * dir.z * CosineA2;\n\
			sh.c[8] = 0.546274 * (dir.x * dir.x - dir.y * dir.y) * CosineA2;\n\
		}\n\
		\n\
		vec3 ComputeSHDiffuse(in vec3 normal)\n\
		{\n\
			SH9Color shs;\n\
			for(int i = 0; i < 9; ++i)\n\
				shs.c[i] = u_sh_coeffs[i];\n\
			\n\
			// Compute the cosine lobe in SH, oriented about the normal direction\n\
			SH9 shCosine;\n\
			SHCosineLobe(normal, shCosine);\n\
			\n\
			// Compute the SH dot product to get irradiance\n\
			vec3 irradiance = vec3(0.0);\n\
			const int num = 9;\n\
			for(int i = 0; i < num; ++i)\n\
				irradiance += radiance.c[i] * shCosine.c[i];\n\
			\n\
			vec3 shDiffuse = irradiance * (1.0 / Pi);\n\
			\n\
			return irradiance;\n\
		}\n\
	';

	HDRE.Filter_Shader_Vertex = '\
		precision highp float;\n\
		attribute vec2 a_coord;\n\
		uniform float u_ioffset;\n\
		uniform float u_blocks;\n\
		varying vec3 v_dir;\n\
		varying vec2 v_coord;\n\
		void main() {\n\
			vec2 uv = a_coord;\n\
			uv.x /= u_blocks;\n\
			uv.x += u_ioffset;\n\
			v_coord = uv;\n\
			v_dir = vec3( uv - vec2(0.5), 0.5 );\n\
			//v_dir.y = -v_dir.y;\n\
			gl_Position = vec4(vec3(a_coord * 2.0 - 1.0, 0.5), 1.0);\n\
		}\n\
	';

	HDRE.Filter_Shader_Fragment = '\
		#extension GL_EXT_shader_texture_lod : enable\n\
		precision highp float;\n\
		\n\
		uniform samplerCube u_color_texture;\n\
		uniform mat3 u_cameras[6]; \n\
		uniform mat3 u_rotation;\n\
		uniform float u_roughness;\n\
		uniform vec4 u_viewport; \n\
		\n\
		uniform float u_mipCount;\n\
		uniform float u_emsize;\n\
		varying vec3 v_dir;\n\
		varying vec2 v_coord;\n\
		const float PI = 3.1415926535897932384626433832795;\n\
		const float size = 512.0;\n\
		void main() {\n\
			vec3 N = normalize( u_rotation * v_dir );\n\
			vec4 color = vec4(0.0);\n\
			float roughness = clamp(u_roughness, 0.0045, 0.98);\n\
			float alphaRoughness = roughness * roughness;\n\
			float lod = clamp(roughness * u_mipCount, 0.0, u_mipCount);\n\
			const float step = 2.0;\n\
			float cfs = u_emsize / pow(2.0, lod);\n\
			\n\
			for(float i = 0.5; i < size; i+=step)\n\
			for(float j = 0.5; j < size; j+=step) {\n\
				if(i > u_emsize || j > u_emsize)\n\
				break;\n\
				// Get pixel\n\
				vec2 r_coord = vec2(i, j) / vec2(u_emsize, u_emsize);\n\
				// Get 3d vector\n\
				vec3 dir = vec3( r_coord - vec2(0.5), 0.5 );\n\
				\n\
				// Use all faces\n\
				for(int iface = 0; iface < 6; iface++) {\n\
					\n\
					mat3 _camera_rotation = u_cameras[iface];\n\
					vec3 pixel_normal = normalize( _camera_rotation * dir );\n\
					float dotProduct = max(0.0, dot(N, pixel_normal));\n\
					float glossScale = 8.0;\n\
					float glossFactor = (1.0 - roughness );\n\
					float cmfs = u_emsize/pow(2.0, lod);\n\
					float weight = pow(dotProduct, cmfs * glossFactor * glossScale );\n\
					if(weight > 0.0 ) {\n\
						color.rgb += textureCube(u_color_texture, pixel_normal).rgb * weight;\n\
						color.a += weight;\n\
					}\n\
				}\n\
			}\n\
			float invWeight = 1.0/color.a;\n\
			gl_FragColor = vec4(color.rgb * invWeight, 1.0);\n\
		}\n\
	';

	HDRE.CopyCubemap_Shader_Fragment = '\
		precision highp float;\n\
		varying vec2 v_coord;\n\
		uniform vec4 u_color;\n\
		uniform vec4 background_color;\n\
		uniform vec3 u_camera_position;\n\
		uniform samplerCube u_color_texture;\n\
		uniform mat3 u_rotation;\n\
		uniform bool u_flip;\n\
		void main() {\n\
			vec2 uv = vec2( v_coord.x, 1.0 - v_coord.y );\n\
			vec3 dir = vec3( uv - vec2(0.5), 0.5 );\n\
			dir = u_rotation * dir;\n\
			if(u_flip)\n\
				dir.x *= -1.0;\n\
			gl_FragColor = textureCube(u_color_texture, dir);\n\
		}\n\
	';

	HDRE.Spheremap_Shader_Fragment = '\
		precision highp float;\n\
		varying vec2 v_coord;\n\
		uniform vec4 u_color;\n\
		uniform vec4 background_color;\n\
		uniform vec3 u_camera_position;\n\
		uniform sampler2D u_color_texture;\n\
		uniform mat3 u_rotation;\n\
		vec2 getSphericalUVs(vec3 dir)\n\
		{\n\
			dir = normalize(dir);\n\
			float d = sqrt(dir.x * dir.x + dir.y * dir.y);\n\
			float r = 0.0;\n\
			if(d > 0.0)\n\
				r = 0.159154943 * acos(dir.z) / d;\n\
			float u = 0.5 + dir.x * (r);\n\
			float v = 0.5 + dir.y * (r);\n\
			return vec2(u, v);\n\
		}\n\
		\n\
		void main() {\n\
			vec2 uv = vec2( v_coord.x, v_coord.y );\n\
			vec3 dir = vec3( uv - vec2(0.5), 0.5 );\n\
			dir = u_rotation * dir;\n\
			dir = -dir;\n\
			dir.x = -dir.x;\n\
			vec2 spherical_uv = getSphericalUVs( dir );\n\
			vec4 color = texture2D(u_color_texture, spherical_uv);\n\
			gl_FragColor = color;\n\
		}\n\
	';

	HDRE.LatLong_Shader_Fragment = '\
		precision highp float;\n\
		varying vec2 v_coord;\n\
		uniform vec4 u_color;\n\
		uniform vec4 background_color;\n\
		uniform vec3 u_camera_position;\n\
		uniform sampler2D u_color_texture;\n\
		uniform int u_face;\n\
		#define PI 3.1415926535897932384626433832795\n\
		\n\
		vec3 uvToXYZ(int face, vec2 uv)\n\
		{\n\
			if(face == 0)\n\
				return vec3(    -1.0,   uv.y,     uv.x);\n\
			else if(face == 1)\n\
				return vec3(     1.0,   uv.y,     -uv.x);\n\
			else if(face == 2) // -Y \n\
				return vec3(    -uv.x,   -1.0,     -uv.y);\n\
			else if(face == 3) // +Y \n\
				return vec3(    -uv.x,    1.0,    uv.y);\n\
			else if(face == 4) // +Z \n\
				return vec3(    -uv.x,   uv.y,    -1.0);\n\
			else // -Z \n\
				return vec3(    uv.x,   uv.y,     1.0);\n\
		}\n\
		\n\
		vec2 dirToUV(vec3 dir)\n\
		{\n\
			return vec2(\n\
				0.5 + 0.5 * atan(dir.x, -dir.z) / PI,\n\
				1.0 - acos(dir.y) / PI);\n\
		}\n\
		\n\
		void main() {\n\
			vec2 texCoordNew = v_coord*2.0-1.0;\n\
			vec3 scan = uvToXYZ(u_face, texCoordNew);\n\
			vec3 direction = normalize(scan);\n\
			vec2 src = dirToUV(direction);\n\
			vec4 color = texture2D(u_color_texture, src);\n\
			gl_FragColor = color;\n\
		}\n\
	';

	/* 
		Private library methods
	*/
	function parseSignature( buffer, offset ) {

		var uintBuffer = new Uint8Array( buffer );
		var endOffset = 4;

		return window.TextDecoder !== undefined ? new TextDecoder().decode(new Uint8Array( buffer ).slice( offset, offset + endOffset )) : "";
	}

	function parseString( buffer, offset ) {

		var uintBuffer = new Uint8Array( buffer );
		var endOffset = 0;

		while ( uintBuffer[ offset + endOffset ] != 0 ) 
			endOffset += 1;

		return window.TextDecoder !== undefined ? new TextDecoder().decode(new Uint8Array( buffer ).slice( offset, offset + endOffset )) : "";
	}

	function parseFloat32( buffer, offset, LE ) {
	
		var Float32 = new DataView( buffer.slice( offset, offset + 4 ) ).getFloat32( 0, LE );
		return Float32;
	}

	function parseUint16( buffer, offset, LE ) {
	
		var Uint16 = new DataView( buffer.slice( offset, offset + 2 ) ).getUint16( 0, LE );
		return Uint16;
	}
	
	function parseFaces( size, width, height, pixelData )
    {
        var faces = [],
            it = 0,
            F = HDRE.CUBE_MAP_NEGATIVE_Y;
    
        for(var i = 0; i < 6; i++)
            faces[i] = new Float32Array(size);
    
        // get 3 vertical faces
        for(var i = 0; i < height; i++)
        {
            var x1_n = (width * 0.25) + (i * width),
                    x2_n = (width * 0.5) + (i * width);
    
            if( i === (height / 3) ) { F = HDRE.CUBE_MAP_POSITIVE_Z; it = 0; }
            if( i === (height / 3) * 2 ) { F = HDRE.CUBE_MAP_POSITIVE_Y; it = 0; }
    
            var line = pixelData.subarray(x1_n * 3, x2_n * 3);
            faces[F].set(line, it);
            it += line.length;
        }
    
        // from now get the rest from left to right
    
        it = 0;
        F = HDRE.CUBE_MAP_NEGATIVE_X; // next face
        for(var i = (height / 3); i < (height / 3) * 2; i++)
        {
            var x1_n = (width * 0.0) + (i * width),
                    x2_n = (width * 0.25) + (i * width);
    
            var line = pixelData.subarray(x1_n * 3, x2_n * 3);
            faces[F].set(line, it);
            it += line.length;
        }
    
        it = 0;
        F = HDRE.CUBE_MAP_POSITIVE_X; // next face
        for(var i = (height / 3); i < (height / 3) * 2; i++)
        {
                var x1_n = (width * 0.5) + (i * width),
                        x2_n = (width * 0.75) + (i * width);
    
                var line = pixelData.subarray(x1_n * 3, x2_n * 3);
                faces[F].set(line, it);
                it += line.length;
        }
    
        it = 0;
        F = HDRE.CUBE_MAP_NEGATIVE_Z; // next face
        for(var i = (height / 3); i < (height / 3) * 2; i++)
        {
                var x1_n = (width * 0.75) + (i * width),
                        x2_n = (width * 1.0) + (i * width);
    
                var line = pixelData.subarray(x1_n * 3, x2_n * 3);
                faces[F].set(line, it);
                it += line.length;
        }

        // order faces
        var ret = [];

        ret.push( faces[HDRE.CUBE_MAP_POSITIVE_X],
                faces[HDRE.CUBE_MAP_POSITIVE_Y],
                faces[HDRE.CUBE_MAP_POSITIVE_Z],
                faces[HDRE.CUBE_MAP_NEGATIVE_X],
                faces[HDRE.CUBE_MAP_NEGATIVE_Y],
                faces[HDRE.CUBE_MAP_NEGATIVE_Z] );

        return ret;
    }
	
//footer
})( typeof(window) != "undefined" ? window : (typeof(self) != "undefined" ? self : global ) );
	
// HDR PARSER

//Code ported by Marcin Ignac (2014)
//Based on Java implementation from
//https://code.google.com/r/cys12345-research/source/browse/hdr/image_processor/RGBE.java?r=7d84e9fd866b24079dbe61fa0a966ce8365f5726
var radiancePattern = "#\\?RADIANCE"
var commentPattern = "#.*"
var gammaPattern = "GAMMA=";
var exposurePattern = "EXPOSURE=\\s*([0-9]*[.][0-9]*)";
var formatPattern = "FORMAT=32-bit_rle_rgbe";
var widthHeightPattern = "-Y ([0-9]+) \\+X ([0-9]+)";

//http://croquetweak.blogspot.co.uk/2014/08/deconstructing-floats-frexp-and-ldexp.html
function ldexp(mantissa, exponent) {
    return exponent > 1023 // avoid multiplying by infinity
        ? mantissa * Math.pow(2, 1023) * Math.pow(2, exponent - 1023)
        : exponent < -1074 // avoid multiplying by zero
        ? mantissa * Math.pow(2, -1074) * Math.pow(2, exponent + 1074)
        : mantissa * Math.pow(2, exponent);
}

function readPixelsRawRLE(buffer, data, offset, fileOffset, scanline_width, num_scanlines) {
    var rgbe = new Array(4);
    var scanline_buffer = null;
    var ptr;
    var ptr_end;
    var count;
    var buf = new Array(2);
    var bufferLength = buffer.length;

    function readBuf(buf) {
      var bytesRead = 0;
      do {
        buf[bytesRead++] = buffer[fileOffset];
      } while(++fileOffset < bufferLength && bytesRead < buf.length);
      return bytesRead;
    }

    function readBufOffset(buf, offset, length) {
      var bytesRead = 0;
      do {
        buf[offset + bytesRead++] = buffer[fileOffset];
      } while(++fileOffset < bufferLength && bytesRead < length);
      return bytesRead;
    }

    function readPixelsRaw(buffer, data, offset, numpixels) {
        var numExpected = 4 * numpixels;
        var numRead = readBufOffset(data, offset, numExpected);
        if (numRead < numExpected) {
            throw new Error('Error reading raw pixels: got ' + numRead + ' bytes, expected ' + numExpected);
        }
    }

    while (num_scanlines > 0) {
      if (readBuf(rgbe) < rgbe.length) {
        throw new Error("Error reading bytes: expected " + rgbe.length);
      }

      if ((rgbe[0] != 2)||(rgbe[1] != 2)||((rgbe[2] & 0x80) != 0)) {
          //this file is not run length encoded
          data[offset++] = rgbe[0];
          data[offset++] = rgbe[1];
          data[offset++] = rgbe[2];
          data[offset++] = rgbe[3];
          readPixelsRaw(buffer, data, offset, scanline_width*num_scanlines-1);
          return;
      }

      if ((((rgbe[2] & 0xFF)<<8) | (rgbe[3] & 0xFF)) != scanline_width) {
        throw new Error("Wrong scanline width " + (((rgbe[2] & 0xFF)<<8) | (rgbe[3] & 0xFF)) + ", expected " + scanline_width);
      }

      if (scanline_buffer == null) {
        scanline_buffer = new Array(4*scanline_width);
      }

      ptr = 0;
      /* read each of the four channels for the scanline into the buffer */
      for (var i=0; i<4; i++) {
        ptr_end = (i+1)*scanline_width;
        while(ptr < ptr_end) {
          if (readBuf(buf) < buf.length) {
            throw new Error("Error reading 2-byte buffer");
          }
          if ((buf[0] & 0xFF) > 128) {
            /* a run of the same value */
            count = (buf[0] & 0xFF) - 128;
            if ((count == 0) || (count > ptr_end - ptr)) {
              throw new Error("Bad scanline data");
            }
            while(count-- > 0)
              scanline_buffer[ptr++] = buf[1];
          }
          else {
            /* a non-run */
            count = buf[0] & 0xFF;
            if ((count == 0) || (count > ptr_end - ptr)) {
              throw new Error("Bad scanline data");
            }
            scanline_buffer[ptr++] = buf[1];
            if (--count > 0) {
              if (readBufOffset(scanline_buffer, ptr, count) < count) {
                throw new Error("Error reading non-run data");
              }
              ptr += count;
            }
          }
        }
      }

      /* copy byte data to output */
      for(var i = 0; i < scanline_width; i++) {
        data[offset + 0] = scanline_buffer[i];
        data[offset + 1] = scanline_buffer[i+scanline_width];
        data[offset + 2] = scanline_buffer[i+2*scanline_width];
        data[offset + 3] = scanline_buffer[i+3*scanline_width];
        offset += 4;
      }

      num_scanlines--;
    }

}

//Returns data as floats and flipped along Y by default
function parseHdr(buffer) {
    if (buffer instanceof ArrayBuffer) {
        buffer = new Uint8Array(buffer);
    }

    var fileOffset = 0;
    var bufferLength = buffer.length;

    var NEW_LINE = 10;

    function readLine() {
        var buf = "";
        do {
            var b = buffer[fileOffset];
            if (b == NEW_LINE) {
                ++fileOffset
                break;
            }
            buf += String.fromCharCode(b);
        } while(++fileOffset < bufferLength);
        return buf;
    }

    var width = 0;
    var height = 0;
    var exposure = 1;
    var gamma = 1;
    var rle = false;

    for(var i=0; i<20; i++) {
        var line = readLine();
        var match;
        if (match = line.match(radiancePattern)) {
        }
        else if (match = line.match(formatPattern)) {
            rle = true;
        }
        else if (match = line.match(exposurePattern)) {
            exposure = Number(match[1]);
        }
        else if (match = line.match(commentPattern)) {
        }
        else if (match = line.match(widthHeightPattern)) {
            height = Number(match[1]);
            width = Number(match[2]);
            break;
        }
    }

    if (!rle) {
        throw new Error("File is not run length encoded!");
    }

    var data = new Uint8Array(width * height * 4);
    var scanline_width = width;
    var num_scanlines = height;

    readPixelsRawRLE(buffer, data, 0, fileOffset, scanline_width, num_scanlines);

    //TODO: Should be Float16
    var floatData = new Float32Array(width * height * 4);
    for(var offset=0; offset<data.length; offset += 4) {
        var r = data[offset+0]/255;
        var g = data[offset+1]/255;
        var b = data[offset+2]/255;
        var e = data[offset+3];
        var f = Math.pow(2.0, e - 128.0)

        r *= f;
        g *= f;
        b *= f;

        var floatOffset = offset;

        floatData[floatOffset+0] = r;
        floatData[floatOffset+1] = g;
        floatData[floatOffset+2] = b;
        floatData[floatOffset+3] = 1.0;
    }

    return {
        shape: [width, height],
        exposure: exposure,
        gamma: gamma,
        data: floatData
    }
}

// HDRE ENGINE

//main namespace
(function(global){

    /**
     * Main namespace
     * @namespace HDRTool
     */

    var GL = _GL;

    if(!GL)
    throw( "HDRTool.js needs litegl.js to work" );
    
    
    var HDRTool = global.HDRTool = RT = {

        version: 2.0,
        webglContext: GL.create({ width: window.innerWidth, height: window.innerHeight }),
		FIRST_PASS: true,	
		LOAD_STEPS: 0,
		CURRENT_STEP: 0,

        // ldr stuff (not used since I'm using the other method)
        files_loaded: [],
        files_to_load: 0,
        log_exposure_times: [],
        hdr_min: new Float32Array(3),
        hdr_max: new Float32Array(3),
        hdr_avg: new Float32Array(3),
        tmp_avg: new Float32Array(3),
		max_radiance: 50,

        cubemap_upload_options: {no_flip: false},
        spheremap_upload_options: {}
    };
    
    HDRTool.setup = function(o)
    {
        o = o || {};
        if(HDRTool.configuration)
            throw("setup already called");
        HDRTool.configuration = o;
    }

	HDRTool.default_progress = function( loaded )
	{
        $(".pbar").css("width", (loaded)/5 * 100 + "%");
        $("#import-bar").css('width', (loaded)/5 * 100 + "%");
	}

    /**
    * Read exr file and run the EXRLoader
    * @method load
    * @param {string} file
    * @param {Object} options
    * @param {Function} onprogress
    */
    HDRTool.load = function(file, options, onprogress)
    {
        var options = options || {};
        var that = this;
        var tex_name = this.getName( file ); 

        var onload = function( buffer, options )
        {            
            var options = options || {};

            // File has been dropped
            if(options.filename)
                tex_name = options.filename;

            var result = null, image = null;

            //Load the HDRE directly or create it using HDREBuilder
            if(isHDRE(tex_name)) {
			
				var found = that.parseHDRE( buffer, tex_name, onprogress );
				if(!found) {
					throw("reading error");
				}
			}
            else if(isEXR(tex_name) || isRadiance(tex_name))
            {
                if(!that.Builder)
                    that.Builder = new HDRE.HDREBuilder();

                // image is saved in the Builder pool
                image = that.Builder.fromHDR(tex_name, buffer, options.size);

                // store texture
                result = image.texture;
                gl.textures[ tex_name ] = result;
            }
            else
                throw("file format not accepted");
           
            if(options.oncomplete) // do things when all is loaded
                options.oncomplete( result, image ? image.id : undefined );
        }

        Texture.setUploadOptions( { no_flip: false } );

        // no read is necessary
        if(options.data)
            onload(options.data, options);
        else
            this.request({ url: file, dataType: 'arraybuffer', success: onload, options: options });
    }

    /**
    * Parse the input data and create texture
    * @method parseHDRE
    * @param {ArrayBuffer} buffer 
    * @param {String} tex_name
    * @param {Function} onprogress
    */
    HDRTool.parseHDRE = function( buffer, tex_name, onprogress )
    {
        var onprogress = onprogress || this.default_progress;

        if(!this.Builder)
        this.Builder = new HDRE.HDREBuilder();

        var image = this.Builder.fromFile(buffer, {onprogress: onprogress});

		if(!image)
			return false;

        printVersion( image.version );

        var texture = image.toTexture();

		if(this.core) {
			this.core.setUniform("is_rgbe", image.is_rgbe);
            this.core.setUniform("mipCount", 5);
            
            // new HDRE does not have all the mipmap chain
            delete RM.shader_macros[ 'MIP_COUNT' ];

            if(image.shs)
            {
                RM.registerComponent( IrradianceCache, "IrradianceCache"); 
                this.core.setUniform( "sh_coeffs", image.shs );
            }
            
        }

        gl.textures[tex_name] = texture;
		return true;
    }

	/**
    * Generate cubemap using 6 faces
    * @method cubemapFromImages
    * @param {Object} options
    */
    HDRTool.cubemapFromImages = function(images, options)
    {
		var faces = [];

		for(var i in images) {

			var img = images[i];

			faces.push( img.rgba8 );

			
            /*var tex = new GL.Texture( img.Width, img.Height, {pixel_data: img.rgba8, format: gl.RGBA} );
			gl.textures["fromImages_" + i] = tex;*/

		}

		var options = {
            format: gl.RGBA,
            texture_type: GL.TEXTURE_CUBE_MAP,
            pixel_data: faces
        };

		//Texture.setUploadOptions( {no_flip: true} );

		var tex = new Texture(images[0].Width, images[0].Height, options);
		var name = "@cubemap-" + uidGen.generate();
        gl.textures[name] = tex;
        
        // this.downloadTexture(name);
		
		// reset texture options
		//Texture.setUploadOptions( {no_flip: false} );

        if(this.core)
		    this.core.set(name);    
	}

    /**
    * Precalculate different ems blurred for different roughness values 
    * @method prefilter
    * @param {string} image
    * @param {Object} options (shader, oncomplete, data)
    */
    HDRTool.prefilter = function(texture, options)
    {
		if(!this.FIRST_PASS)
		{
			if(options.oncomplete)
                options.oncomplete();
			return;
		}

		console.warn("Filtering cubemap");
		
        var options = options || {};

		var tex = texture;
		var tex_name = options.name || "texture_prefilter";

		if(texture.constructor === String)
		{
			tex_name = this.getName( texture );
	        tex = gl.textures[tex_name];
		}
        
        var that = this;

        var inner = function( tex, image_id )
        {
            if(!that.Builder)
            that.Builder = new HDRE.HDREBuilder();

            options["image_id"] = image_id;

            that.Builder.filter(tex, options);
        };

        if(!tex)
        {
            var params = {oncomplete: inner};
			var filename = texture;
            
            if(options.data)
                params['data'] = options.data;
            if(options.size)
                params['size'] = options.size;
            
            this.load( filename, params );        
        }
        else
            inner( tex );
    }

    /**
    * Environment BRDF (Store it in a 2D LUT)
    * @method brdf
	* @param {String} path
    */
    HDRTool.brdf = function(path)
    {
		var tex_name = '_brdf_integrator';
		var options = { type: gl.FLOAT, texture_type: gl.TEXTURE_2D, filter: gl.LINEAR};
		
		if(path)
		{
			gl.textures[tex_name] = renderer.loadTexture(path, options);
			return;
		}
        
		var shader = new GL.Shader(HDRTool.BRDF_VSHADER, HDRTool.BRDF_FSHADER);
        var tex = gl.textures[tex_name] = new GL.Texture(128, 128, options);

		var hammersley_tex = gl.textures["hammersley_sample_texture"];
		if(!hammersley_tex && window.Tools)
			hammersley_tex = Tools.create_hammersley_sample_texture();

        tex.drawTo(function(texture) {
    
            if(hammersley_tex)
			    hammersley_tex.bind(0);

            shader.uniforms({
				
				'u_hammersley_sample_texture': 0

			}).draw(Mesh.getScreenQuad(), gl.TRIANGLES);

            if(hammersley_tex)
			    hammersley_tex.unbind();
        });
    }

    /**
    * Returns name of the texture from path name removing all before last "/"
    * @method getName
    * @param {string} path
    */
    HDRTool.getName = function( path )
    {
        var tokens = path.split("/");
        return tokens[ tokens.length - 1 ];
    }

    /**
    * Write an HDRE file to store the cubemap and its roughness levels
    * @method getSkybox
    * @param {String} name file name
    * @param {Object} options
    */
    HDRTool.getSkybox = function( name, options )
    {
		options = options || {};
        
        var texture = gl.textures[ name ];
        var temp = null;
        var width = texture.width;
        var height = texture.height;
		var isRGBE = false;
		var array = Float32Array;

		if(options.type && options.type.BYTES_PER_ELEMENT) // if has this property is a typedArray
		{
			array = options.type;

			// Float32Array cant be rgbe
			if(options.rgbe !== undefined)
			{
				isRGBE = options.rgbe;
			}
		}
            
        var originalSkybox = this.processSkybox( temp ? temp : texture, isRGBE );
        var data = [ originalSkybox ];

        // Get all mips
        for(var i = 1; i < 6; i++)
        {
            data.push( {

                width: texture.width / Math.pow(2, i),
                height: texture.height / Math.pow(2, i),
                pixelData: texture.mipmap_data[i]
            } )
        }

		var write_options = {
			type: array, 
            rgbe: isRGBE,
            channels: options.channels || 3
		}

        return HDRE.write( data, width, height, write_options );
    }

    /**
    * Get info of a texture (pixel data per face, width and height )
    * @method processSkybox
    */
    HDRTool.processSkybox = function( e, isRGBE )
    {
        if(!gl)
        throw( 'no webgl' );

        if(e.constructor === String)
            e = gl.textures[ e ];

        if(!e)
        throw( 'no stored texture with name ' + e );

        var info = {
			width: e.width, 
			height: e.height, 
			pixelData: []
		};

        for(var i = 0; i < 6; i++) {

			// get data for each face
			var faceData = e.getPixels(i);
            info.pixelData.push( isRGBE ? faceData.toRGBE() : faceData);
		}

        return info;
    }

    /**
    * Opens a texture in a new window to download
    * @method download
    * @param {string} name
    */
    HDRTool.downloadTexture = function(name, new_tab)
    {
        var tex = name.constructor === GL.Texture ? name : gl.textures[name];
        if(!tex) {
            console.error("no texture named " + name);
            return;
        }
        var canvas = tex.toCanvas(null, true);
        canvas.style.height = "100%";
        var a = document.createElement("a");
        a.download = name + ".png";
        a.href = canvas.toDataURL();
        
        if(!new_tab)
            a.click();
        else
        {
            a.title = "Texture image";
            a.appendChild(canvas);
            var new_window = window.open();
            new_window.document.title.innerHTML = "Texture image";
            new_window.document.body.appendChild(a);
            new_window.focus();
        }
    }

    /**
    * Request file by XMLHTTPRequest
    * @method request
    * @param {Object} request
    */
    HDRTool.request = function(request)
	{
		var dataType = request.dataType || "text";
		if(dataType == "json") //parse it locally
			dataType = "text";
		else if(dataType == "xml") //parse it locally
			dataType = "text";
		else if (dataType == "binary")
		{
			dataType = "arraybuffer";
			request.mimeType = "application/octet-stream";
		}	

		//regular case, use AJAX call
        var xhr = new XMLHttpRequest();
        xhr.open( request.data ? 'POST' : 'GET', request.url, true);
        if(dataType)
            xhr.responseType = dataType;
        if (request.mimeType)
            xhr.overrideMimeType( request.mimeType );
		if( request.nocache )
			xhr.setRequestHeader('Cache-Control', 'no-cache');

        xhr.onload = function(load)
		{
			var response = this.response;
			if(this.status != 200)
			{
				var err = "Error " + this.status;
				if(request.error)
					request.error(err);
				return;
			}

			if(request.success)
				request.success.call(this, response, request.options ? request.options : null);
        };
        
        xhr.onerror = function(err) {
			if(request.error)
				request.error(err);
        }
        
        xhr.onprogress = function(e) {
			$("#xhr-load").css("width", parseFloat( (e.loaded)/e.total * 100 ) + "%");
		}

        xhr.send();
		return xhr;
    }

	/**
    * Assemble HDR image from bracketed stack of ldr images
    * @method assembleHDR_HDRGPU
    * @param {} 
    */
	HDRTool.assembleHDR_HDRGPU = function(hdr_scale)
	{

        if(!this.core)
        return;

		delete gl.textures["CROPPED"]; // reset previous cropped 
		delete gl.textures["CROPPED_MIP"]; 
		console.time('assembly');

		var images = this.files_loaded;
		this._sortFiles();

		var that = this;
		var ext = gl.extensions["EXT_sRGB"];

		if(!ext)
			throw("EXT_sRGB not supported");

		const numImages = images.length;
		const width = images[0].Width;
		const height = images[0].Height;

		// output texture
		var hdr = new GL.Texture( width, height, { type: GL.FLOAT, format: GL.RGBA} );
		var mipmaps_assembled = new GL.Texture( nearestPowerOfTwo(width), nearestPowerOfTwo(height), { type: GL.FLOAT, format: GL.RGBA, minFilter: GL.LINEAR_MIPMAP_LINEAR} );

        var stack           = [];
        var ExposureTimes   = [];
        var uniforms        = {
            u_numImages: numImages,
            u_ExposureTimes: [],
            u_WhiteBalance: [],
            u_hdr_scale: hdr_scale
        };

		for(var i = 0; i < numImages; i++)
		{
            images[i].texture.bind(i);
            uniforms["u_ExposureTimes"].push( images[i].ExposureTime[0] );
            if(images[i].verbose)
                uniforms["u_WhiteBalance"].push( new Float32Array( images[i].verbose.multipliers ) );
            uniforms["u_stack" + i] = i;
        }
        
        // first is raw?
        if(images[0].verbose)
        uniforms["u_WhiteBalance"] = GL.linearizeArray( uniforms["u_WhiteBalance"] );

		var shader = gl.shaders['HDRassembly'];
		if(!shader)
        throw("no shader");
        
		hdr.drawTo(function(){

			shader.uniforms(uniforms).draw(Mesh.getScreenQuad(), gl.TRIANGLES);

        });
        
        mipmaps_assembled.drawTo(function(){

			shader.uniforms(uniforms).draw(Mesh.getScreenQuad(), gl.TRIANGLES);

		});

		for(var i = 0; i < numImages; i++)
            images[i].texture.unbind(); 

	    HDRI.changeScale( 855 / width );

		mipmaps_assembled.bind(0);
		gl.generateMipmap(gl.TEXTURE_2D);
		mipmaps_assembled.unbind();

		gl.textures["ASSEMBLED"] = hdr;
		gl.textures["ASSEMBLED_MIP"] = mipmaps_assembled;

		console.timeEnd('assembly');
	}
    
    HDRTool.parseCR2 = function( buffer, name )
    {
        if(!parserCR2)
        throw("cr2 parser missing");

        
        //parserCR2.parse(buffer, name, parserCR2.ONLY_METADATA );

        var dcraw_options = { 
				
            verbose: true, 					// -v 
            use16BitLinearMode: true, 		// -6 -W -g 1 1
            //setCustomGammaCurve: "1 1",                       // no gamma -> linear
            //setNoAutoBrightnessMode: true,	// -W
            useCameraWhiteBalance: true, 	// -w
            //useCustomWhiteBalance: "1 1 0 0",
            setInterpolationQuality: 2,
            exportAsTiff: true				// -T 
        };

        return parserCR2.parse(buffer, name, dcraw_options);
    }

    HDRTool.parseJPG = function( buffer, name )
    {
        var decoder = new JpegImage();
        decoder.parse( new Uint8Array(buffer) );

        var w = decoder.width, h = decoder.height;
        var data = decoder.getData(w, h);

        return {
            rgba8: this.addAlphaChannel(data),
            Width: w,
            Height: h,
            data: data
        };
    }
    
    HDRTool.parsePNG = function( buffer, name )
    {
        var img  = UPNG.decode(buffer);        // put ArrayBuffer of the PNG file into UPNG.decode
        img.rgba8 = this.addAlphaChannel(img.data);

        return {
            rgba8: img.rgba8,
            Width: img.width,
            Height: img.height,
        };
    }
    
    HDRTool.parseHDR = function( buffer, name )
    {
        var img = RGBE.parseHdr(buffer);
        
        return {
            rgba8: img.data,
            Width: img.shape[0],
            Height: img.shape[1],
            hdr: true
        };
    }
    
    HDRTool.parseLDRI = function( buffer, name, extension )
    {
        switch(extension){
            
            case 'hdr':
                return this.parseHDR( buffer, name );
            case 'nef':
            case 'cr2':
                return this.parseCR2( buffer, name );
            case 'jpg':
                return this.parseJPG( buffer, name );
            case 'png':
                return this.parsePNG( buffer, name );
        }
    
    }

    HDRTool._sortFiles = function(higher_first)
    {
        this.files_loaded.sort(function(a, b){

            if(higher_first)
            {
                var aux = Object.assign(a);
                a = b;
                b = aux;
            }

            if(a.name.includes("sample-"))
                return parseInt(a.name[7]) - parseInt(b.name[7]);

            if(a.name.includes("DSC_")){

                var an = parseInt( a.name.slice(4, a.name.length - 4) );
                var bn = parseInt( b.name.slice(4, b.name.length - 4) );
                return parseInt(an - bn);
            }
            
            if(a.name.includes("IMG_")){

                var an = parseInt( a.name.slice(4, a.name.length - 4) );
                var bn = parseInt( b.name.slice(4, b.name.length - 4) );
                return parseInt(an - bn);
            }

            if(!a.ExposureTime || !b.ExposureTime)
                console.warn("missing exp times");

            return a.ExposureTime - b.ExposureTime;
        
        });

        this.log_exposure_times.sort(function(a, b){
        
            return a - b;
        
        });
    }
    
    HDRTool.loadLDRI = function( content, extension, name, options )
    {
        options = options || {};
        var that = this;

        var getParams = function (e) {

            var data = e ? e.currentTarget.result : content;
            let img = that.parseLDRI(data, name, extension);

            img.name = name;
            if(img.rgba8)
                img.rgb = that.extractChannels(img.rgba8, img.Width * img.Height);
            img.url = extension == "cr2" ? "assets/CR2_Example.JPG" : URL.createObjectURL( content );

            // fill exposures in case of png or jpg
            if( !img['ExposureTime'] )
                img['ExposureTime'] = 1/(1+that.files_loaded.length);
            
            evaluate(img);
            
        };

        var evaluate = function (img) {
            
            Texture.setUploadOptions( {no_flip: false} );
            var tex;

            var w = img.Width;
            var h = img.Height;

            if(!img.hdr) {

                if(img.BitsPerSample && img.BitsPerSample[0] == 16)
                    tex = new GL.Texture( w, h, {type: GL.FLOAT, pixel_data: img.rgba32, format: gl.RGBA } );
                else
                    tex = new GL.Texture( w, h, {pixel_data: img.rgba8, format: gl.RGBA } );
            
            }
            else
                tex = new GL.Texture( w, h, {pixel_data: img.rgba8, format: gl.RGBA, type: GL.FLOAT } );

            tex.filename = img.name;
            
            gl.textures[img.name] = tex;
            img.texture = tex;
            that.files_loaded.push( img );

            that.files_in_load++;

            var prog_width = (that.files_in_load / that.files_to_load ) * 100;

            $("#xhr-load").css('width', prog_width + "%");
            
            // all files loaded
            if(prog_width == 100)
            { 
                that._sortFiles();

                if(options.callback)
                    options.callback();
            }
        };

        // content is binary
        if(content.constructor === ArrayBuffer)
        {
            //processFile();
            console.warn('TODO');

        // content is a file
        }else{

            var reader = new FileReader();
            reader.onload = getParams;
            reader.readAsArrayBuffer(content);
        }
    }

    HDRTool.getUniforms = function()
    {
        var uniforms = {
                u_hdr_avg: this.hdr_avg,
                u_tmp_avg: this.tmp_avg,
            
                u_hdr_min: this.hdr_min,
                u_hdr_max: this.hdr_max,
            
                u_max_lum_pixel: this.max_lum_pixel,
                u_max_lum: this.max_lum,
            
                u_min_lum_pixel: this.min_lum_pixel,
                u_min_lum: this.min_lum,
            
                u_max_radiance: this.max_radiance

            };

        return uniforms;
    }

    HDRTool.assembleHDR_DEBEVEC = function( images )
    {
        images = images || this.files_loaded;
    
        var that = this;
        const channels = 3;

        const width = images[0].Width;
        const height = images[0].Height;

        // # Loading exposure images into a list

        var exposure_times = [];

        for(var i in images)
        exposure_times.push( images[i].ExposureTime );

        var hdr = this.computeOutput(exposure_times);

        var mipmaps_assembled = new GL.Texture( nearestPowerOfTwo(width), nearestPowerOfTwo(height), { type: GL.FLOAT, format: GL.RGBA, minFilter: GL.LINEAR_MIPMAP_LINEAR} );

        mipmaps_assembled.drawTo( function() {
            renderer.clear( that.core._background_color );
            Object.assign( renderer._uniforms, HDRTool.getUniforms() );
            hdr.toViewport();
        });

        mipmaps_assembled.bind(0);
        gl.generateMipmap(gl.TEXTURE_2D);
        mipmaps_assembled.unbind();

        gl.textures["ASSEMBLED"] = hdr;
        gl.textures["ASSEMBLED_MIP"] = mipmaps_assembled;

        
    }

    /*
        This method computes the final HDR image with the radiance map 
        of every image channel 

        Returns array(channels) of array(w*h)
    */
    HDRTool.computeOutput = function( ExposureTimes )
    {
        var images = this.files_loaded;
    
        var that = this;
        const channels = 3;
        const smoothing_lambda = 100;

        const width = images[0].Width;
        const height = images[0].Height;

        var hdr_image = new Float32Array( width * height * channels );
        this.hdr_image = hdr_image;

        // shader: normalize, tonemap, adjust, normalize

        this.log_exposure_times = ExposureTimes;

        console.time('compute');

        // python version
        for( var ch = 0; ch < channels; ch++ )
        {
            // this is the data of all the images per channel
            var layer_stack = [];
            for(var i = 0; i < images.length; i++) layer_stack.push( images[i].rgb[ch] );
            
            // now we want to get the intensities for each image (of a channel) in the layer stack
            var intensity_samples = this.sampleIntensitiesLayer( layer_stack, width);
            var response_curve = this.computeResponseCurveLayer( intensity_samples, layer_stack, smoothing_lambda );
            var radiance_map = this.computeRadianceMapLayer( layer_stack, response_curve, width, height, ch);
            // final step, fill hdr image with each radiance_map
            this.composeImageLayer( radiance_map, hdr_image, ch );
        }

        // save lum

        /*this.max_lum = -Infinity;
        this.min_lum = Infinity;
        this.max_lum_pixel = null;
        this.min_lum_pixel = null;

        for( var i = 0; i < this.hdr_image.length; )
        {
            var color = [this.hdr_image[i++],this.hdr_image[i++],this.hdr_image[i++]];
            var lum = 0.2126 * color[0] + 0.7152 * color[1] + 0.0722 * color[2];
            
            if(lum > this.max_lum)
            {
                this.max_lum = lum;
                this.max_lum_pixel = color;
            }
            else if(lum < this.min_lum)
            {
                this.min_lum = lum;
                this.min_lum_pixel = color;
            }
                
        }*/

        console.timeEnd('compute');

        var hdr_tex = new GL.Texture( width, height, {type: GL.FLOAT, pixel_data: hdr_image, format: gl.RGB } );

        return hdr_tex;
    }

    HDRTool._GPU_downloadHDR = function()
    {
        var max_radiance = this.max_radiance; // ojo con this
        var ldr_tex = gl.textures["combined"];
        var norm_tex = gl.textures["combined_scaled"] = new GL.Texture( ldr_tex.width, ldr_tex.height, {type: GL.FLOAT, format: gl.RGB } );
        var shader = gl.shaders['combineHDR'];

        if(!shader)
        throw("no shader");

        var that = this;

        console.time('download');

        ldr_tex.bind(0);
        norm_tex.drawTo(function(){

            shader.uniforms({
                u_hdr_avg: that.hdr_avg,
                u_tmp_avg: that.tmp_avg,
            
                u_hdr_min: that.hdr_min,
                u_hdr_max: that.hdr_max,
            
                u_max_lum_pixel: that.max_lum_pixel,
                u_max_lum: that.max_lum,
            
                u_min_lum_pixel: that.min_lum_pixel,
                u_min_lum: that.min_lum,
            
                u_max_radiance: max_radiance

            }).draw(Mesh.getScreenQuad(), gl.TRIANGLES);

        });
        ldr_tex.unbind();

        console.timeEnd('download');
        return norm_tex;
    }

    /*
        This method normalizes the final HDR image in the CPU
    */
    HDRTool._CPU_downloadHDR = function()
    {
        var hdr_image = this.hdr_image;
        var new_data = new Float32Array(hdr_image.length);
        var max_radiance = this.max_radiance;
        
        console.time('download');

        for( var i = 0; i < hdr_image.length; i+=3 )
        {
            var pixel = [hdr_image[i], hdr_image[i+1], hdr_image[i+2]];

            // linear normalizing to 0-1
            pixel = this.normalize(pixel);

            // scale (not linear) to max intensity (100, 200, 300??)
            pixel = this.scaleToRadiance(pixel, max_radiance);

            // adjust with pattern intensities
            pixel = this.adjustIntensity(pixel,2.0, max_radiance);

            new_data[i] = pixel[0];
            new_data[i+1] = pixel[1];
            new_data[i+2] = pixel[2];
        }

        console.timeEnd('download');

        this.texture_data = new_data;
        var ldr_tex = gl.textures["combined"];
        return gl.textures["combined_scaled"] = new GL.Texture( ldr_tex.width, ldr_tex.height, {type: GL.FLOAT, pixel_data: new_data, format: gl.RGB } );
    }

    HDRTool.normalize = function( Xi )
    {
        var pixel = new Float32Array(3);
        var maxLum = this.max_lum_pixel;
        var minLum = this.min_lum_pixel;

        pixel[0] = (1.0)/(maxLum[0]-minLum[0])*(Xi[0]-minLum[0]);
        pixel[1] = (1.0)/(maxLum[1]-minLum[1])*(Xi[1]-minLum[1]);
        pixel[2] = (1.0)/(maxLum[2]-minLum[2])*(Xi[2]-minLum[2]);

        return pixel;
    }

    HDRTool.scaleToRadiance = function( Xi, max_radiance )
    {
        const A = 20.4730;
        const B = 44.9280;
        const C = 36.7912;
        const D = 13.5250;
        const E = 2.47270;
        const F = 0.14253;
        const G = 0.00032;


        var pixel = new Float32Array(3);

        for(var i = 0; i < Xi.length; i++)
            pixel[i] = (A * Math.pow(Xi[i],6.0)
                - B * Math.pow(Xi[i],5.0)
                + C * Math.pow(Xi[i],4.0)
                - D * Math.pow(Xi[i],3.0)
                + E * Math.pow(Xi[i],2.0)
                - F * Xi[i]
                + G) * max_radiance;

        return pixel;
    }

    HDRTool.adjustIntensity = function( Xi, BIAS, max_radiance )
    {
        if(!BIAS)
            BIAS = 1.0;

        var pixel = new Float32Array(3);
        var pattern = this.tmp_avg; // pattern is already in range 0-1
        var average = this.normalize(this.hdr_avg);// this average is not

        //pattern = this.scaleToRadiance(pattern, max_radiance);
        //average = this.scaleToRadiance(average, max_radiance);

        var patternMatch = numeric.div(pattern, average);
        patternMatch = numeric.mul(patternMatch, BIAS);

        pixel[0] = Xi[0] * patternMatch[0];
        pixel[1] = Xi[1] * patternMatch[1];
        pixel[2] = Xi[2] * patternMatch[2];

        return pixel;
    }

    HDRTool.sampleIntensitiesLayer = function( images, width )
    {
        if(!images)
            throw('bad params');

        const z_min = 0, z_max = 255;
        const num_intensities = z_max - z_min + 1;
        const num_images = images.length;

        // Find the middle image to use as the source for pixel intensity locations
        var mid_img = images[( num_images / 2 )|0];

        // compute size of ocurrence vector
        var rows = new Uint32Array( 1 );
        var cols = new Uint32Array( 1 );

        var intensities = [];

        for( var j = 0; j < num_images; j++ ) {

            var intensity_values = new Uint8Array(num_intensities);
            var img = images[j];
    
            for(var i = 0; i < num_intensities; i++) {

                // very slow if no max ocurrences defined
                var num_rows = this.FastgetXYFromArray( mid_img, width, i, rows, cols, 1);
                
                if(!num_rows)
                    continue;

                var idx = 0;//(Math.random() * num_rows)|0;
                
                var index1D = width * rows[idx] + cols[idx];
                var value = img[ index1D ];
                intensity_values[i] = value;
            }
            
            // push each channel intensity for each image
            intensities.push( intensity_values );
        }

        return intensities;
    }

    /*
    Parameters
    ----------
    smoothing_lambda : float
        A constant value used to correct for scale differences between
        data and smoothing terms in the constraint matrix
    Returns
    -------
        Return a vector g(z) where the element at index i is the log exposure
        of a pixel with intensity value z = i (e.g., g[0] is the log exposure
        of z=0, g[1] is the log exposure of z=1, etc.)
    */
    HDRTool.computeResponseCurveLayer = function(intensity_samples, images, smoothing_lambda)
    {
        var z_min = 0, z_max = 255, 
        intensity_range = z_max - z_min,
        smoothing_lambda = smoothing_lambda || 100;

        const num_samples = intensity_range+1;
        const num_images = images.length;
        const log_exposure_times = this.log_exposure_times;

        if(log_exposure_times.length < num_images){

            LiteGUI.alert("Log exposures times missing", {title: "error"});
            throw("no enough log exposures");
        }
        
        var aN = num_images * num_samples + intensity_range;
        var aM = num_samples + intensity_range + 1;
        var mat_A = new Float64Array( aN * aM );// [ aN, aM ] -> inv: 	[ aM, aN ]
        var mat_b = new Float64Array( aN * 1 ); // [ aN,  1 ] -> 		[ aN,  1 ] can be multiplied! later!!
        
        // 1. Add data-fitting constraints:
        var k = 0;

        for(var i = 0; i < num_samples; i++) {

            for( var j = 0; j < num_images; j++ ) {

                // mat[i][j] == array[width*j+i]
                var z_ij = intensity_samples[j][i];
                var w_ij = this.linearWeight(z_ij);
                var iMa1 = aM * k + z_ij;
                var iMa2 = aM * k + ((intensity_range + 1) + i);
                mat_A[ iMa1 ] = w_ij;
                mat_A[ iMa2 ] = -w_ij;
                mat_b[ k ] = w_ij * log_exposure_times[j];
                k++;
            }
        }

        // 2. Add smoothing constraints:
        for(var z_k = (z_min + 1); z_k < z_max; z_k++) {

            var w_k = this.linearWeight(z_k);
            var iMa1 = aM * k + (z_k - 1);
            var iMa2 = aM * k + (z_k);
            var iMa3 = aM * k + (z_k + 1);
            mat_A[ iMa1] = w_k * smoothing_lambda;
            mat_A[ iMa2 ] = -2 * w_k * smoothing_lambda;
            mat_A[ iMa3 ] = w_k * smoothing_lambda;
            k++;
        }

        // 3. Add color curve centering constraint:
        var constraint = (intensity_range/2)|0;
        var iMa = aM * k + constraint;
        mat_A[ iMa ] = 1;

        // create A from mat_A array
        var A = this.listToMatrix(mat_A, aM);
        var B = this.listToMatrix(mat_b, 1);

        var inv_A = numbers.matrix.pinv(A); // pseudo-inverse (numeric.js and linearAlgebra.js)

        var x = numbers.matrix.multiply(inv_A, B);
        var g = x.slice( 0, intensity_range + 1 );

        return GL.linearizeArray( g, Float64Array );
    }
    /*
        """Calculate a radiance map for each pixel from the response curve.
        Parameters
        ----------
        images : list
        response_curve : list
        weighting_function : Function
        Returns
        -------
        array(float64)
            The image radiance map (in log space)
        """
    */
    HDRTool.computeRadianceMapLayer = function(images, response_curve, width, height, channel)
    {
        // matrix of image w, h
        var num_images = images.length;
        var img_rad_map = new Float32Array(width * height);

        var log_exposure_times = this.log_exposure_times;
        var curves = new Float32Array(num_images);
        var weights = new Float32Array(num_images);

        // Find the middle image to use as the source for pixel intensity locations
        var mid_img = images[( num_images / 2 )|0];
        var avg = 0;

        for(var i = 0; i < width; i++)
        for(var j = 0; j < height; j++) {

            var index = height * i + j;

            // get here template average????
            avg += mid_img[index];


            for( var k = 0; k < num_images; k++ ) {
                var img_data = images[k];
                curves[k] = response_curve[ img_data[index] ];
                weights[k] = this.linearWeight( img_data[index] );
            }

            var SumW = weights.reduce((a, b) => a + b, 0);

            if(SumW > 0) {
                var A = numeric.div( numeric.mul(weights, numeric.sub(curves, log_exposure_times)), SumW );
                var value = A.reduce((a, b) => a + b, 0);
                img_rad_map[index] = value;
            }
            else
            {
                var imi = (num_images/2)|0;
                var value = curves[ imi ] - log_exposure_times[ imi ];
                img_rad_map[index] = value;
            }
        }

        this.tmp_avg[channel] = (avg/(width*height))/255.0;
        return img_rad_map;
    }

    /**
     */
    HDRTool.composeImageLayer = function( radiance_map, hdr_image, channel )
    {
        var num_channels = 3;

        // go through all radiance map as i
        // K begins in channel and k+=channel
        var k = channel;

        // save from here the max and min values
        var min = Infinity;
        var max = -Infinity;
        var avg = 0;

        for( var i = 0; i < radiance_map.length; i++){

            var value = radiance_map[i];
            
            hdr_image[k] = value;
            k+=num_channels;

            // save min, max, avg
            avg += value;
            if(value < min) min = value;
            else if(value > max) max = value;
        }

        this.hdr_min[channel] = min;
        this.hdr_max[channel] = max;
        this.hdr_avg[channel] = avg/radiance_map.length;
    }

    HDRTool.addAlphaChannel = function( array, value )
    {
        value = value || 255;
        var new_size = array.length + array.length/3;
        var data = new array.constructor(new_size);
        var k = 0;

        for( var i = 0; i < new_size;  )
        {
            data[i++] = array[k++];
            data[i++] = array[k++];
            data[i++] = array[k++];
            data[i++] = value;
        }

        return data;
    }

    // extract channels from RGBA
    HDRTool.extractChannels = function( img_data, size)
    {
        var values = [];
        img_data = img_data.slice(0, size*4); // get only valid pixels

        for( var n = 0; n < 3; n++ ) {

            var new_data = new Uint8Array( size );

            for(var i = n, id = 0; i < img_data.length; i+=4, id++)
            {
                new_data[id] = img_data[i];
            }
            values.push( new_data );
        }
        
        return values;
    }

    HDRTool.FastgetXYFromArray = function( array, width, prof, rows, cols, max_p) {

        var r = array.length;
        var index = 0;
    
        for(var i = 0; i < r; i++) {
    
            if(array[i] == prof) {
                rows[index] = ( i/width )|0;
                cols[index] = i % width;
                
                ++index;
    
                if(index > max_p)
                    return index;
            }
        }
    
        return index;
    }

    HDRTool.listToMatrix = function(list, elementsPerSubArray) {
        var matrix = [], i, k;
    
        for (i = 0, k = -1; i < list.length; i++) {
            if (i % elementsPerSubArray === 0) {
                k++;
                matrix[k] = [];
            }
    
            matrix[k].push(list[i]);
        }
    
        return matrix;
    }

    /*"""	Linear weighting function based on pixel intensity that reduces the
            weight of pixel values that are near saturation.
    """*/
    HDRTool.linearWeight = function( value )
    {
        var z_min = 0, z_max = 255;

        if( value <= ((z_min + z_max) / 2))
            return value - z_min;
        return z_max - value;
    }

    /* 
        Private methods used in parsing steps
    */

    function printVersion( v )
    {
        console.log( '%cHDRE v'+v, 'padding: 3px; background: rgba(0, 0, 0, 0.75); color: #6E6; font-weight: bold;' );
    }

    function isHDRE( texture_name )
    {
        return texture_name.toLowerCase().includes(".hdre");
    }

    function isEXR( texture_name )
    {
        return texture_name.toLowerCase().includes(".exr");
    }

    function isRadiance( texture_name )
    {
        return texture_name.toLowerCase().includes(".hdr");
    }
    
    // https://stackoverflow.com/questions/5678432/decompressing-half-precision-floats-in-javascript
    
    HDRTool.decodeFloat16 = decodeFloat16;
    
    function decodeFloat16( binary ) {
    
        var exponent = ( binary & 0x7C00 ) >> 10,
        fraction = binary & 0x03FF;
    
        return ( binary >> 15 ? - 1 : 1 ) * (
        exponent ?
            (
            exponent === 0x1F ?
                fraction ? NaN : Infinity :
                Math.pow( 2, exponent - 15 ) * ( 1 + fraction / 0x400 )
            ) :
            6.103515625e-5 * ( fraction / 0x400 )
        );
    
    }

    HDRTool.decodeFloat16 = decodeFloat16;

    if(window.numbers)
    {
        numbers.matrix.pinv = function(M) {

            if(M.length < M[0].length)
                return linalg.transposeSync(linalg.pinvSync(linalg.transposeSync(M)))
            else
                return linalg.pinvSync(M)
        }
    }
	

	// http://locutus.io/c/math/frexp/
	Math.frexp = function(arg) {

	  arg = Number(arg)

	  const result = [arg, 0]

	  if (arg !== 0 && Number.isFinite(arg)) {
		const absArg = Math.abs(arg)
		// Math.log2 was introduced in ES2015, use it when available
		const log2 = Math.log2 || function log2 (n) { return Math.log(n) * Math.LOG2E }
		let exp = Math.max(-1023, Math.floor(log2(absArg)) + 1)
		let x = absArg * Math.pow(2, -exp)

		while (x < 0.5) {
		  x *= 2
		  exp--
		}
		while (x >= 1) {
		  x *= 0.5
		  exp++
		}

		if (arg < 0) {
		  x = -x
		}
		result[0] = x
		result[1] = exp
	  }
	  return result
	}

	Math.ldexp = function(mantissa, exponent) {
		var steps = Math.min(3, Math.ceil(Math.abs(exponent) / 1023));
		var result = mantissa;
		for (var i = 0; i < steps; i++)
			result *= Math.pow(2, Math.floor((exponent + i) / steps));
		return result;
	}


	// https://github.com/OpenImageIO/oiio/blob/master/src/hdr.imageio/rgbe.cpp
	function rgbe2float( rgbe )
	{
		var f;

		if (rgbe[3] > 0) {   /*nonzero pixel*/
			f = Math.ldexp(1.0, rgbe[3] - (128+8));
			rgbe[0] *= f;
			rgbe[1] *= f;
			rgbe[2] *= f;
			rgbe[3] = 1;
		}
		else {
			rgbe[0] = rgbe[1] = rgbe[2] = 0;
			rgbe[3] = 1;
		}

		return rgbe;
	}

	function float2rgbe(x, y, z)
	{
		var m, e;
		var rgbe = new Float32Array(4);
		var r, g, b;

		if(y === undefined && z === undefined) {
			// x is a vector
			if(x.length < 3)
				throw("bad params")
			r = x[0];
			g = x[1];
			b = x[2];
		}
		else {
			r = x;
			g = y;
			b = z;
		}

		var v = Math.max(r, g, b);

		if(isNaN(v)) {
		
			console.log(x, y, z);
			console.log(r, g, b);
			throw("NaN");
		
		}
		
		if (v == 0.0) {
			rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
		}
		else {
			[m, e] = Math.frexp(v);
			v = m * (256.0 / v);
			rgbe[0] = parseInt((r * v));
			rgbe[1] = parseInt((g * v));
			rgbe[2] = parseInt((b * v));
			rgbe[3] = parseInt((e + 128));
		}

		return rgbe;
	}

	Float32Array.prototype.toRGBE = function(){

		var length = this.length;
		var data = new Uint8Array( length );
		
		for( var i = 0; i < length; i+=4 )
		{
			var rgbei = float2rgbe( this[i],this[i+1],this[i+2] );
			data[i] = rgbei[0];
			data[i+1] = rgbei[1];
			data[i+2] = rgbei[2];
			data[i+3] = rgbei[3];
		}
		
		return data;

	}

	Uint8Array.prototype.toFloat = function(){

		var length = this.length;
		var data = new Float32Array( length );

		for( var i = 0; i < length; i+=4 )
		{
			var floated = rgbe2float( [this[i],this[i+1],this[i+2],this[i+3]] );
			data[i] = floated[0];
			data[i+1] = floated[1];
			data[i+2] = floated[2];
			data[i+3] = 1.0;
		}
		
		return data;

	}

	HDRTool.COPY_CUBEMAP_FSHADER = `
		
		precision highp float;
		varying vec2 v_coord;
		uniform vec4 u_color;
		uniform vec4 background_color;
		uniform vec3 u_camera_position;
		uniform samplerCube u_color_texture;
		uniform mat3 u_rotation;
		uniform bool u_flip;

		void main() {

			vec2 uv = vec2( v_coord.x, 1.0 - v_coord.y );
			vec3 dir = vec3( uv - vec2(0.5), 0.5 );
			dir = u_rotation * dir;

			if(u_flip)
				dir.x *= -1.0;

			gl_FragColor = textureCube(u_color_texture, dir);
		}
	
	`;

	HDRTool.SPHERE_MAP_FSHADER = `
		
		precision highp float;
		varying vec2 v_coord;
		uniform vec4 u_color;
		uniform vec4 background_color;
		uniform vec3 u_camera_position;
		uniform sampler2D u_color_texture;
		uniform mat3 u_rotation;

		vec2 getSphericalUVs(vec3 dir)
		{
			dir = normalize(dir);
			dir = -dir;
			float d = sqrt(dir.x * dir.x + dir.y * dir.y);
			float r = 0.0;

			if(d > 0.0)
				r = 0.159154943 * acos(dir.z) / d;

	    		float u = 0.5 + dir.x * (r);
			float v = 0.5 + dir.y * (r);

			return vec2(u, v);
		}

		void main() {

			vec2 uv = vec2( v_coord.x, 1.0 - v_coord.y );
			vec3 dir = vec3( uv - vec2(0.5), 0.5 );
			dir = u_rotation * dir;
			
			dir.x = -dir.x;

			// use dir to calculate spherical uvs
			vec2 spherical_uv = getSphericalUVs( dir );
			vec4 color = texture2D(u_color_texture, spherical_uv);
			gl_FragColor = color;
		}
	
	`;

	HDRTool.LATLONG_MAP_FSHADER = `
		
		precision highp float;
		varying vec2 v_coord;
		uniform vec4 u_color;
		uniform vec4 background_color;
		uniform vec3 u_camera_position;
		uniform sampler2D u_color_texture;
		uniform mat3 u_rotation;

		#define PI 3.1415926535897932384626433832795

		vec2 getPanoramicUVs(vec3 dir)
		{
			dir = -normalize(dir);

	    		float u = 1.0 + (atan(dir.x, -dir.z) / PI);
			float v = acos(-dir.y) / PI;

			return vec2(u/2.0, v);
		}

		void main() {

			vec2 uv = vec2( v_coord.x, 1.0 - v_coord.y );
			vec3 dir = vec3( uv - vec2(0.5), 0.5 );
			dir = u_rotation * dir;

			vec2 panoramic_uv = getPanoramicUVs( dir );
			vec4 color = texture2D(u_color_texture, panoramic_uv);
			gl_FragColor = color;
		}
	
	`;

	HDRTool.BRDF_SAMPLING_SHADERCODE = `

		/* -- Tangent Space conversion -- */
		vec3 tangent_to_world(vec3 vector, vec3 N, vec3 T, vec3 B)
		{
		  return T * vector.x + B * vector.y + N * vector.z;
		}
		vec2 noise2v(vec2 co)  {
		    return vec2(
				fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453),
				fract(sin(dot(co.yx ,vec2(12.9898,78.233))) * 43758.5453)
			);
		}
		float noise(vec2 co)  {
		    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
		}
		vec3 sample_ggx(vec3 rand, float a2)
		{
		  /* Theta is the aperture angle of the cone */
		  float z = sqrt((1.0 - rand.x) / (1.0 + a2 * rand.x - rand.x)); /* cos theta */
		  float r = sqrt(max(0.0, 1.0 - z * z));                        /* sin theta */
		  float x = r * rand.y;
		  float y = r * rand.z;

		  /* Microfacet Normal */
		  return vec3(x, y, z);
		}
		vec3 hammersley_3d(float i, float invsamplenbr)
		{
		  vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

		  Xi.x = i * invsamplenbr; /* i/samples */
		  Xi.x = fract(Xi.x + jitternoise.x);

		  int u = int(mod(i + jitternoise.y * HAMMERSLEY_SIZE, HAMMERSLEY_SIZE));

		  Xi.yz = texture2D(u_hammersley_sample_texture, vec2(u, 0)).rg;

		  return Xi;
		}
		vec2 Hammersley(const in int index, const in int numSamples){
			vec2 r = fract(vec2(float(index) * 5.3983, float(int(int(2147483647.0) - index)) * 5.4427));
			r += dot(r.yx, r.xy + vec2(21.5351, 14.3137));
			return fract(vec2(float(index) / float(numSamples), (r.x * r.y) * 95.4337));
		}
		vec3 sample_ggx(float nsample, float a2, vec3 N, vec3 T, vec3 B)
		{
			vec3 Xi = vec3(
				Hammersley(int(nsample), sampleCount),
				0.0
			);
			// Xi = hammersley_3d(nsample, float(1.0/float(sampleCount)));
			vec3 Ht = sample_ggx(Xi, a2);
			return tangent_to_world(Ht, N, T, B);
		}
		float G1_Smith_GGX(float NX, float a2)
		{
		  /* Using Brian Karis approach and refactoring by NX/NX
		   * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
		   * Rcp is done on the whole G later
		   * Note that this is not convenient for the transmission formula */
		  return NX + sqrt(NX * (NX - NX * a2) + a2);
		  /* return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function */
		}
		
	`;

	HDRTool.BRDF_VSHADER = `
		
		precision highp float;

		attribute vec3 a_vertex;
		attribute vec3 a_normal;
		attribute vec2 a_coord;

		varying vec2 v_coord;
		varying vec3 v_vertex;

		void main(){
			v_vertex = a_vertex;
			v_coord  = a_coord;
			vec3 pos = v_vertex * 2.0 - vec3(1.0);
			gl_Position = vec4(pos, 1.0);
		}
	`;

	HDRTool.BRDF_FSHADER = `

		// BLENDER METHOD
		precision highp float;
		varying vec2 v_coord;
		varying vec3 v_vertex;
		vec2 jitternoise = vec2(0.0);

		uniform sampler2D u_hammersley_sample_texture;

		#define sampleCount 8192
		#define PI 3.1415926535897932384626433832795
		
		const float HAMMERSLEY_SIZE = 8192.0; 

		`  +  HDRTool.BRDF_SAMPLING_SHADERCODE +  `
		
		 void main() {

			vec3 N, T, B, V;

			float NV = ((clamp(v_coord.y, 1e-4, 0.9999)));
			float sqrtRoughness = clamp(v_coord.x, 1e-4, 0.9999);
			float a = sqrtRoughness * sqrtRoughness;
			float a2 = a * a;

			N = vec3(0.0, 0.0, 1.0);
			T = vec3(1.0, 0.0, 0.0);
			B = vec3(0.0, 1.0, 0.0);
			V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

			// Setup noise (blender version)
			jitternoise = noise2v(v_coord);

			 /* Integrating BRDF */
			float brdf_accum = 0.0;
			float fresnel_accum = 0.0;
			for (int i = 0; i < sampleCount; i++) {
				vec3 H = sample_ggx(float(i), a2, N, T, B); /* Microfacet normal */
				vec3 L = -reflect(V, H);
				float NL = L.z;

				if (NL > 0.0) {
					float NH = max(H.z, 0.0);
					float VH = max(dot(V, H), 0.0);

					float G1_v = G1_Smith_GGX(NV, a2);
					float G1_l = G1_Smith_GGX(NL, a2);
					float G_smith = 4.0 * NV * NL / (G1_v * G1_l); /* See G1_Smith_GGX for explanations. */

					float brdf = (G_smith * VH) / (NH * NV);
					float Fc = pow(1.0 - VH, 5.0);

					brdf_accum += (1.0 - Fc) * brdf;
					fresnel_accum += Fc * brdf;
				}
			}

			brdf_accum /= float(sampleCount);
			fresnel_accum /= float(sampleCount);

			gl_FragColor = vec4(brdf_accum, fresnel_accum, 0.0, 1.0);
		}
	`;

    //footer
    
    })( typeof(window) != "undefined" ? window : (typeof(self) != "undefined" ? self : global ) );
    
    
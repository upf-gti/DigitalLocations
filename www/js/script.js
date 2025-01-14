import { LX } from 'lexgui';

function _processVector( vector )
{
    var array = [];
    for( var i = 0; i < vector.size(); ++i )
        array.push( vector.get(i) );

    return array;
}

window.App = {

    dragSupportedExtensions: [ /*'hdr'*/, 'glb', 'ply' ],

    messageTypes: {
        0: "PARAMETER_UPDATE",
        1: "LOCK",
        2: "SYNC",
        3: "PING",
        4: "RESEND_UPDATE",
        5: "UNDO_REDO_ADD",
        6: "RESET_OBJECT",
        7: "RPC",
        255: "EMPTY"
    },

    parameterTypes: {
        0: "NONE",
        1: "ACTION",
        2: "BOOL",
        3: "INT",
        4: "FLOAT",
        5: "VECTOR2",
        6: "VECTOR3",
        7: "VECTOR4",
        8: "QUATERNION",
        9: "COLOR",
        10: "STRING",
        11: "LIST",
        100: "UNKNOWN"
    },

    init() {

        this.cameraTypes = [ "Orbit", "Flyover" ];
        this.cameraNames = [ ];

        this.cameraSpeed = 1.0;

        this.urlParams = new URLSearchParams( window.location.search );

        this.location = this.urlParams.get( 'location' );
        this.dev = this.urlParams.has( 'dev' ) ? JSON.parse( this.urlParams.get( 'dev' ) ) : false;
        this.ui = this.urlParams.has( 'ui' ) ? JSON.parse( this.urlParams.get( 'ui' ) ) : true;

        this.initUI();

        if( this.location )
        {
            this.toggleModal( true );

            const ext = LX.getExtension( this.location );
            switch( ext ) {
                case 'glb': this.loadLocation.call( this, this._loadGltf, this.location ); break;
                case 'ply': this.loadLocation.call( this, this._loadPly, this.location ); break;
            }
        }

        // this.loadedCounter = 0;

        // const onSuccess = ( data, request ) => {

        //     const array = new Int8Array( data );
        //     const byteSize = array.length * array.BYTES_PER_ELEMENT;
        //     const ptr = Module._malloc( byteSize );

        //     Module.HEAP8.set( array, ptr );

        //     const url = request.responseURL;
        //     const funcName = `set_scene_${ url.substr( url.lastIndexOf( '.' ) + 1 ) }`;
        //     const func = Module.cwrap( funcName, "void", [ "number", "number" ] );

        //     func( ptr, byteSize );

        //     Module._free( ptr );

        //     this.loadedCounter++;

        //     if (this.loadedCounter == 3) {
        //         window.engineInstance.loadTracerScene();
        //     }
        // };

        // LX.requestBinary( "https://emil-xr.eu/dl/DigitalLocation.materials", onSuccess );
        // LX.requestBinary( "https://emil-xr.eu/dl/DigitalLocation.nodes", onSuccess );
        // LX.requestBinary( "https://emil-xr.eu/dl/DigitalLocation.objects", onSuccess );
        // LX.requestBinary( "https://emil-xr.eu/dl/DigitalLocation.textures", onSuccess );

        // For scene request
        // {
        //     const distributor = new zmq.socket("rep");

        //     distributor.on('message', function( msg ) {
        //         let string = new TextDecoder().decode( msg );
        //         console.log( string );
        //         setTimeout( () => distributor.send("Hello C++, sending scene!"), 1000 );
        //     });

        //     distributor.connect("ws://127.0.0.1:5501");
        // }

        // For scene updates
        {
            const subscriber = new zmq.socket("sub");

            subscriber.subscribe("");

            subscriber.options.onconnect = () => {
                console.log("Connected!");
            };

            subscriber.on('message', ( msg ) => {
                this.onReceiveMessage( msg );
            });

            subscriber.connect("ws://127.0.0.1:5506");
        }

    },

    initUI() {

        var area = LX.init();

        var canvas = document.getElementById( "canvas" );
        area.attach( canvas );

        canvas.addEventListener('dragenter', e => e.preventDefault() );
        canvas.addEventListener('dragleave', e => e.preventDefault() );
        canvas.addEventListener('drop', (e) => {
            e.preventDefault();
            this.toggleModal( true );
            const file = e.dataTransfer.files[0];
            const ext = LX.getExtension( file.name );
            if( this.dragSupportedExtensions.indexOf( ext ) == -1 )
                return;
            switch( ext ) {
                // case 'hdr': this.loadEnvironment( file ); break;
                case 'glb': this.loadLocation( this._loadGltf, file ); break;
                case 'ply': this.loadLocation( this._loadPly, file ); break;
            }
        });

        canvas.addEventListener('keyup', e => {
            if( e.key == 'c' )
            {
                e.preventDefault();
                this.toggleUI();
            }
        } );

        // Create loading  modal

        this.modal = document.createElement( 'div' );

        this.modal.style.width = "100%";
        this.modal.style.height = "100%";
        this.modal.style.opacity = "0.9";
        this.modal.style.backgroundColor = "#000";
        this.modal.style.position = "absolute";
        this.modal.style.cursor = "wait";
        this.modal.hidden = true;

        area.attach( this.modal );

        this.dialog = new LX.PocketDialog( "Control Panel", p => {

            this.panel = p;

            const onBeforeRead = () => this.toggleModal( true );

            if( this.dev )
            {
                p.branch( "Digital Location", { closed: true } );
                p.addText( "Name", "", null, { signal: "@location_name", disabled: true } );
                // p.addFile( "Load", (data, file) => this.loadGltf( file, data ), { type: 'buffer', local: false, onBeforeRead: onBeforeRead } );
                p.addCheckbox( "Rotate", false, () => window.engineInstance.toggleSceneRotation() );
            
                /*
                p.branch( "Environment", { closed: true } );
                p.addText( "Name", "", null, { signal: "@environment_name", disabled: true } );
                p.addFile( "Load", (data, file) => this.loadEnvironment( file, data ), { type: 'buffer', local: false, onBeforeRead: onBeforeRead } );
                */

                p.branch( "Camera", { closed: true } );
            }
            else
            {
                p.addTitle( "Camera" );
            }

            p.addDropdown( "Type", this.cameraTypes, "Orbit", (value) => this.setCameraType( value ) );
            p.addNumber( "Speed", this.cameraSpeed, (value) => this.setCameraSpeed( value ), { min: 0.1, max: 5.0, step: 0.1 } );
            p.addList( "Look at", this.cameraNames, "", (value) => this.lookAtCameraIndexFromName( value ) );
            p.addButton( null, "Reset", () => this.resetCamera() );
        
        }, { size: [300, null], float: "right", draggable: false });

        if( !this.ui )
        {
            this.toggleUI();
        }
    },

    onReceiveMessage( msg ) {

        // let string = new TextDecoder().decode( msg );
        const buffer = msg;

        var offset = 0;
        const clientID = buffer.readUInt8( offset ); offset += 1;
        const time = buffer.readUInt8( offset ); offset += 1;
        const messageType = buffer.readUInt8( offset ); offset += 1;

        if(this.messageTypes[ messageType ] == "PARAMETER_UPDATE")
        {
            console.log( buffer );
            console.log("Client ID: " + clientID);
            console.log("Time: " + time);
            console.log("Message Type: " + this.messageTypes[ messageType ]);

            this.readSceneParameter( buffer, offset );
        }
    },

    readSceneParameter( buffer, offset ) {

        while ( offset < buffer.length ) {

            const sceneID = buffer.readUInt8( offset ); offset += 1;
            const sceneObjectID = buffer.readUInt16LE( offset ) - 1; offset += 2;

            console.log(`Scene ${ sceneID }, object ${ sceneObjectID }`);

            const parameterID = buffer.readUInt16LE( offset ); offset += 2;
            const parameterType = buffer.readUInt8( offset ); offset += 1;
            const parameterLength = buffer.readUInt32LE( offset ); offset += 4;

            console.log(`Parameter ${ parameterID }, of type ${ this.parameterTypes[ parameterType ] } (${ parameterLength })`);

            // Note: We don't have here the scene from VPET, so the node list
            // and the object_id is unknown..

            // assert(scene_object_id < vpet.node_list.size());
            // sVPETNode* vpet_node = vpet.editables_node_list[scene_object_id];
            // Node3D* node_ref = vpet_node->node_ref;

            // if (!node_ref) {
            //     return;
            //     zmq_msg_close(&message);
            // }

            let value = null;

            switch ( this.parameterTypes[ parameterType ] )
            {
            case "FLOAT": {
                value = buffer.readFloatLE( offset ); offset += 4;
                break;
            }
            case "VECTOR2": {
                value = [
                    buffer.readFloatLE( offset ),
                    buffer.readFloatLE( offset + 4 )
                ]
                offset += 8;
                break;
            }
            case "VECTOR3": {
                value = [
                    buffer.readFloatLE( offset ),
                    buffer.readFloatLE( offset + 4 ),
                    buffer.readFloatLE( offset + 8 )
                ]
                offset += 12;
                break;
            }
            case "COLOR": {
                value = [
                    buffer.readFloatLE( offset ),
                    buffer.readFloatLE( offset + 4 ),
                    buffer.readFloatLE( offset + 8 ),
                    buffer.readFloatLE( offset + 12 )
                ]
                offset += 16;
                break;
            }
            case "QUATERNION": {
                value = [
                    buffer.readFloatLE( offset ),
                    buffer.readFloatLE( offset + 4 ),
                    buffer.readFloatLE( offset + 8 ),
                    buffer.readFloatLE( offset + 12 )
                ]
                offset += 16;
                break;
            }
            default:
                console.assert( 0 );
                break;
            }

            console.log( `Value ${ value }` );

            switch ( parameterID ) {
            case 0: // POSITION: -z
                value[2] = -value[2];
                // node_ref->set_position(vector3);
                break;
            case 1: // ROTATION: -x and -y
                value[0] = -value[0];
                value[1] = -value[1];
                // node_ref->set_rotation(rotation);
                break;
            case 2: // SCALE
                // node_ref->set_scale(vector3);
                break;
            case 3: // LIGHT_COLOR
                // if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                //     Light3D* light_ref = static_cast<Light3D*>(node_ref);
                //     light_ref->set_color(vector4);
                // }
                break;
            case 4: // LIGHT_INTENSITY
                // if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                //     Light3D* light_ref = static_cast<Light3D*>(node_ref);
                //     light_ref->set_intensity(f32);
                // }
                break;
            case 5: // LIGHT_RANGE
                // if (vpet_node->node_type == eVPETNodeType::LIGHT) {
                //     Light3D* light_ref = static_cast<Light3D*>(node_ref);
                //     light_ref->set_range(f32 * 2.0f);
                // }
                break;
            default:
                console.assert( 0 );
            }
        }

    },

    toggleModal( force ) {

        this.modal.hidden = force !== undefined ? (!force) : !this.modal.hidden;
    },

    toggleUI( force ) {

        this.dialog.root.hidden = force !== undefined ? (!force) : !this.dialog.root.hidden;
    },

    setCameraType( type ) {

        console.log( "Setting " + type + " Camera" );

        const index = this.cameraTypes.indexOf( type );

        window.engineInstance.setCameraType( index );
    },

    setCameraSpeed( value ) {

        window.engineInstance.setCameraSpeed( value );
    },

    resetCamera() {

        if( this.cameraNames.length )
        {
            this.lookAtCameraIndexFromName( this.cameraNames[ 0 ] );
        }
        else
        {
            window.engineInstance.resetCamera();
        }
    },

    lookAtCameraIndexFromName( name ) {

        console.log( "Look at " + name );

        const index = this.cameraNames.indexOf( name );

        window.engineInstance.setCameraLookAtIndex( index );
    },

    // parseEnvironment( name, data ) {

    //     if( data.constructor === File )
    //     {
    //         const reader = new FileReader();
    //         reader.readAsArrayBuffer( data );
    //         reader.onload = e => this.parseEnvironment( data.name, e.target.result );
    //         return;
    //     }

    //     setTimeout( () => {

    //         const params = {

    //             data: data,
    //             size: 256,
    //             filename: name,
    //             oncomplete: () => {
    //                 const buffer = HDRTool.getSkybox( name, {  channels: 4 } );
    //                 this._loadEnvironment( name.replace( ".hdr", ".hdre" ), buffer );
    //             }
    //         };

    //         HDRTool.prefilter( name, params );

    //     }, 150 );
    // },

    // loadEnvironment( file, data ) {

    //     if( LX.getExtension( file.name ) == 'hdr')
    //     {
    //         this.parseEnvironment( null, file );
    //         return;
    //     }

    //     if( !data )
    //     {
    //         const reader = new FileReader();
    //         reader.readAsArrayBuffer( file );
    //         reader.onload = e => this._loadEnvironment( file.name, e.target.result );
    //         return;
    //     }
        
    //     this._loadEnvironment( file.name, data );
    // },

    loadLocation( loader, file, data ) {

        if( !data )
        {
            // file is the path URL
            if( file.constructor == String )
            {
                const path = file;
                LX.requestBinary( path, ( data ) => loader.call(this, path, data ), ( e ) => {
                    LX.popup( e.constructor === String ? e :  `[${ path }] can't be loaded.`, "Request Blocked", { size: ["400px", "auto"], timeout: 10000 } );
                    this.toggleModal( false );
                } );
                return;
            }

            const reader = new FileReader();
            reader.readAsArrayBuffer( file );
            reader.onload = e => loader.call(this, file.name, e.target.result );
            return;
        }
        
        loader.call(this, file.name ?? file, data );
    },

    // _loadEnvironment( name, buffer ) {

    //     name = name.substring( name.lastIndexOf( '/' ) );

    //     console.log( "Loading " + LX.getExtension( name ).toUpperCase(), [ name, buffer ] );

    //     this._fileStore( name, buffer );

    //     // This will load the hdre and set texture to the skybox
    //     window.engineInstance.setEnvironment( name );

    //     this.toggleModal( false );

    //     // Update UI
    //     LX.emit( '@environment_name', name.replace( /.hdre|.hdr/, "" ) );
    // },

    _loadGltf( name, buffer ) {

        name = name.substring( name.lastIndexOf( '/' ) + 1 );
        
        console.log( "Loading glb", [ name, buffer ] );

        this._fileStore( name, buffer );

        var cameraNamesVector = window.engineInstance.loadGLB( name );

        this.toggleModal( false );

        // Update UI

        LX.emit( '@location_name', name.replace( '.glb', '' ) );

        // Update Camera look at points

        this.cameraNames = _processVector( cameraNamesVector );

        this.panel.get( "Look at" ).updateValues( this.cameraNames );

        if( this.cameraNames.length )
        {
            // this.lookAtCameraIndexFromName( this.cameraNames[ 0 ] );
            this.panel.get( "Look at" ).set(this.cameraNames[ 0 ])
        }
    },

    _loadPly( name, buffer ) {

        name = name.substring( name.lastIndexOf( '/' ) + 1 );

        console.log( "Loading ply", [ name, buffer ] );

        this._fileStore( name, buffer );

        window.engineInstance.loadPly( name );

        this.cameraNames = []
        this.panel.get( "Look at" ).updateValues( this.cameraNames );

        this.toggleModal( false );

        // Update UI

        LX.emit( '@location_name', name.replace( '.ply', '' ) );
    },

    _fileStore( filename, buffer ) {

        let data = new Uint8Array( buffer );
        let stream = FS.open( filename, 'w+' );
        FS.write( stream, data, 0, data.length, 0 );
        FS.close( stream );
    }
};


Promise.resolve( Module.Engine.getInstance() ).then( result => {

    if ( !result ) {
        console.error( "Module Instance is null" );
    }

    window.engineInstance = result;
    window.App.init();

} ).catch( error => {
    console.log( error );
});

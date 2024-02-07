import { LX } from 'lexgui';

function _processVector( vector )
{
    var array = [];
    for( var i = 0; i < vector.size(); ++i )
        array.push( vector.get(i) );

    return array;
}

const App = window.App = {

    dragSupportedExtensions: [ 'hdr', 'hdre', 'glb' ],

    init() {

        this.cameraTypes = [ "Flyover", "Orbit" ];
        this.cameraNames = [ ];

        Module.onRuntimeInitialized = () => {
         
            this.initUI();
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
            const file = e.dataTransfer.files[0];
            const ext = LX.getExtension( file.name );
            if( this.dragSupportedExtensions.indexOf( ext ) == -1 )
                return;
            switch( ext ) {
                case 'hdr': this.parseEnvironment( null, file ); break;
                case 'hdre': this.loadEnvironment( file ); break;
                case 'glb': this.loadGltf( file ); break;
            }
        });

        // Create loading  modal

        this.modal = document.createElement( 'div' );

        this.modal.style.width = "100%";
        this.modal.style.height = "100%";
        this.modal.style.opacity = "0.8";
        this.modal.style.backgroundColor = "#000";
        this.modal.style.position = "absolute";
        this.modal.style.display = "none";

        this.toggleModal = () => this.modal.style.display = (this.modal.style.display == "block" ? "none" : "block");

        area.attach( this.modal );

        new LX.PocketDialog( "Control Panel", p => {

            this.panel = p;

            p.branch( "Digital Location", { closed: true } );
            p.addText( "Name", "", null, { signal: "@location_name", disabled: true } );
            p.addFile( "Load", (data, file) => this.loadGltf(data, file), { type: 'buffer', local: false } );
            p.addCheckbox( "Rotate", false, () => Module.Engine.toggleSceneRotation() );
        
            p.branch( "Environment", { closed: true } );
            p.addText( "Name", "", null, { signal: "@environment_name", disabled: true } );
            p.addFile( "Load", (data, file) => this.loadEnvironment(data, file), { type: 'buffer', local: false } );
        
            p.branch( "Camera", { closed: true } );
            p.addDropdown( "Type", this.cameraTypes, "Flyover", (value) => this.setCameraType( value ) );
            p.addList( "Look at", this.cameraNames, "", (value) => this.lookAtCameraIndexFromName( value ) );
        
        }, { size: [300, null], float: "right", draggable: false });

    },

    setCameraType( type ) {

        console.log( "Setting " + type + " Camera" );

        const index = this.cameraTypes.indexOf( type );

        Module.Engine.setCameraType( index );
    },

    lookAtCameraIndexFromName( name ) {

        console.log( "Look at " + name );

        const index = this.cameraNames.indexOf( name );

        Module.Engine.setCameraLookAtIndex( index );
    },

    parseEnvironment( name, data ) {

        if( data.constructor === File )
        {
            const reader = new FileReader();
            reader.readAsArrayBuffer( data );
            reader.onload = e => this.parseEnvironment( data.name, e.target.result );
            return;
        }

        this.toggleModal();

        setTimeout( () => {

            const params = {

                data: data,
                size: 256,
                filename: name,
                oncomplete: () => {
                    const buffer = HDRTool.getSkybox( name, {  channels: 4 } );
                    this._loadEnvironment( name.replace( ".hdr", ".hdre" ), buffer );
                    this.toggleModal();
                }
            };

            HDRTool.prefilter( name, params );

        }, 50 );
    },

    loadEnvironment( data, file ) {

        if( data.constructor === File )
        {
            const reader = new FileReader();
            reader.readAsArrayBuffer( data );
            reader.onload = e => this._loadEnvironment( data.name, e.target.result );
            return;
        }
        
        this._loadEnvironment( file.name, data );
    },

    loadGltf( data, file ) {

        if( data.constructor === File )
        {
            const reader = new FileReader();
            reader.readAsArrayBuffer( data );
            reader.onload = e => this._loadGltf( data.name, e.target.result );
            return;
        }
        
        this._loadGltf( file.name, data );
    },

    async _updateCameraNames() {

        var cameraNamesVector = await Module.Engine.getCameraNames();
        
        this.cameraNames = _processVector( cameraNamesVector );
    },

    _loadEnvironment( name, buffer ) {

        name = name.substring( name.lastIndexOf( '/' ) );

        console.log( "Loading " + LX.getExtension( name ).toUpperCase(), [ name, buffer ] );

        this._fileStore( name, buffer );

        // This will load the hdre and set texture to the skybox
        Module.Engine.setEnvironment( name );

        // Update UI
        LX.emit( '@environment_name', name.replace( /.hdre|.hdr/, "" ) );
    },

    async _loadGltf( name, buffer ) {

        name = name.substring( name.lastIndexOf( '/' ) );
        
        console.log( "Loading glb", [ name, buffer ] );

        this._fileStore( name, buffer );

        Module.Engine.loadGLB( name );

        // Update UI

        LX.emit( '@location_name', name.replace( '.glb', '' ) );

        // Update Camera look at points

        await this._updateCameraNames();

        this.panel.get( "Look at" ).updateValues( this.cameraNames );
    },

    _fileStore( filename, buffer ) {

        let data = new Uint8Array( buffer );
        let stream = FS.open( filename, 'w+' );
        FS.write( stream, data, 0, data.length, 0 );
        FS.close( stream );
    }
};

App.init();
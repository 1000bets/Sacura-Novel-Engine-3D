const base=require('./package.json').build;
module.exports={...base,extends:null,
 directories:{output:'release-windows'},
 files:base.files,
 extraResources:[{from:'../build/windows-msvc-release/ElectronNative/Release',to:'native',filter:['**/*','!**/*.pdb','!**/*Test.exe','!**/*.lib','!**/*.exp','!**/Qt*.dll','!platforms/**/*','!imageformats/**/*']}],
 win:{target:[{target:'zip',arch:['x64']}],signAndEditExecutable:false},
 artifactName:'Sacura-Desktop-${version}-Windows-${arch}.${ext}'
};

import React, {useId} from 'react';

// Decorative illustrations, not screenshots of the templates. Kept as local vectors.
export default function ProjectArtwork({variant='letters', className=''}) {
 const id=useId().replace(/:/g,'');
 const paint=name=>`url(#${id}-${name})`;
 return <svg className={`project-artwork ${className}`} viewBox="0 0 640 360" preserveAspectRatio="xMidYMid slice" fill="none" aria-hidden="true" focusable="false">
  <defs>
   <linearGradient id={`${id}-sky`} x1="320" y1="0" x2="320" y2="360" gradientUnits="userSpaceOnUse"><stop stopColor="#767685"/><stop offset=".6" stopColor="#cfaaa4"/><stop offset="1" stopColor="#edc7b2"/></linearGradient>
   <linearGradient id={`${id}-room`} x1="0" y1="0" x2="640" y2="360" gradientUnits="userSpaceOnUse"><stop stopColor="#242b35"/><stop offset="1" stopColor="#41404b"/></linearGradient>
   <linearGradient id={`${id}-floor`} x1="320" y1="190" x2="320" y2="360" gradientUnits="userSpaceOnUse"><stop stopColor="#4b4b56"/><stop offset="1" stopColor="#292d38"/></linearGradient>
   <linearGradient id={`${id}-gold`} x1="0" y1="0" x2="140" y2="140" gradientUnits="userSpaceOnUse"><stop stopColor="#efd6a3"/><stop offset="1" stopColor="#aa7f52"/></linearGradient>
   <linearGradient id={`${id}-door`} x1="270" y1="100" x2="380" y2="290" gradientUnits="userSpaceOnUse"><stop stopColor="#dab69c"/><stop offset="1" stopColor="#8e6d70"/></linearGradient>
   <pattern id={`${id}-grid`} width="44" height="44" patternUnits="userSpaceOnUse" patternTransform="skewX(-30) scale(1 .5)"><path d="M44 0H0V44" stroke="#cbd0d9" strokeOpacity=".08"/></pattern>
   <pattern id={`${id}-rain`} width="41" height="65" patternUnits="userSpaceOnUse" patternTransform="rotate(12)"><path d="M8 6V24M29 38V47" stroke="#e2dae0" strokeOpacity=".27"/></pattern>
  </defs>
  {variant==='letters'||variant==='novel'?<>
   <path fill={paint('room')} d="M0 0h640v360H0z"/>
   <path d="M0 0h188v238L0 302V0Z" fill="#252c36"/>
   <path d="M188 0h452v238H188z" fill="#41414b"/>
   <path d="M188 238h452v122H0v-58l188-64Z" fill={paint('floor')}/>
   <path d="m188 238 178 122M276 238l266 122M420 238l220 74M0 338h640" stroke="#a5a0a5" strokeOpacity=".12"/>
   <path d="M234 30h273v192H234z" fill="#242d38"/>
   <path d="M245 40h251v170H245z" fill={paint('sky')}/>
   <circle cx="433" cy="83" r="25" fill="#f0d0b8" fillOpacity=".65"/>
   <path d="m245 153 56-33 73 37 49-20 73 37v36H245v-57Z" fill="#626c77"/>
   <path d="m245 181 94-26 72 25 85-21v51H245v-29Z" fill="#475760"/>
   <path d="M245 40h251v170H245z" fill={paint('rain')}/>
   <path d="M369 40v170M245 125h251" stroke="#343944" strokeWidth="7"/>
   <path d="M225 218h291v9H225z" fill="#79717a"/>
   <path d="m234 227 160 93h246v-42l-144-51H234Z" fill="#edc7b2" fillOpacity=".06"/>
   <path d="m145 259 226-34 176 47-227 47-175-60Z" fill="#b99a8c"/>
   <path d="m145 259 175 49 227-36v12l-227 46-175-60v-11Z" fill="#7b6564"/>
   <path d="M165 278v74m156-25v33m204-76v57" stroke="#292a34" strokeWidth="13"/>
   <path d="m304 255 75-11 62 18-74 13-63-20Z" fill="#e6d6c2"/>
   <path d="m304 255 63 7 12-18m-12 18 74 0" stroke="#ad9b92" strokeWidth="1.5"/>
   <path d="m218 256 38-6 29 9-38 7-29-10Z" fill="#494553"/>
   <path d="m220 252 37-6 28 9-37 7-28-10Z" fill="#857180"/>
   <path d="m456 241 18-2 8 6-19 3-7-7Z" fill="#33343c"/>
   <path d="M466 242v-49m0 8-19-18m19 30 15-20" stroke="#9e8f7b" strokeWidth="3"/>
   <path d="M447 190c-16-7-19-20-16-27 13 2 23 10 16 27Zm32 12c-1-18 5-26 17-28 3 13-3 23-17 28Z" fill="#8c9a8c"/>
   <path d="m454 233 25-2-5 17-13 2-7-17Z" fill="#c7aa96"/>
   <path d="M533 65h43v64h-43z" stroke="#80747a" strokeWidth="4"/>
   <path d="m542 109 11-17 14 27h-25Z" fill="#a58b88"/>
   <path d="M80 238v-88m0 27-22-20m22 39 24-28" stroke="#8b8d83" strokeWidth="4"/>
   <ellipse cx="57" cy="150" rx="12" ry="25" transform="rotate(-30 57 150)" fill="#697d75"/>
   <ellipse cx="101" cy="165" rx="12" ry="27" transform="rotate(29 101 165)" fill="#839186"/>
   <ellipse cx="80" cy="139" rx="11" ry="28" fill="#8c9a8c"/>
   <path d="m61 230 39-3-7 42-24 3-8-42Z" fill="#9d7e77"/>
   {variant==='novel'&&<g><path d="m297 93 164 20-8 85-164-20 8-85Z" fill="#272d39" fillOpacity=".9"/><path d="m316 119 113 14m-116 6 82 10m-85 6 105 13" stroke="#c5b6bc" strokeWidth="3"/><circle cx="431" cy="182" r="3" fill="#e4b5ba"/></g>}
  </>:variant==='horror'?<>
   <path fill="#202b32" d="M0 0h640v360H0z"/>
   <path d="M0 0h190l110 120v240H0V0Z" fill="#303d43"/><path d="M640 0H450L340 120v240h300V0Z" fill="#29383f"/>
   <path d="M190 0h260L340 120h-40L190 0Z" fill="#18242c"/><path d="M0 360 300 120h40l300 240H0Z" fill="#465356"/>
   <path d="m70 360 238-240m267 240L332 120M183 268h275M245 202h148M278 158h82" stroke="#87918a" strokeOpacity=".22"/>
   <path d="M292 78h58v105h-58z" fill="#101b21"/><path d="M300 91h42v92h-42z" fill="#51796c"/><path d="M329 137h5v5h-5z" fill="#e2bc74"/>
   <path d="M411 130h39v53h-39z" fill="#875348"/><path d="M416 138h29v6h-29z" fill="#cc9b79"/>
   <path d="m97 222 97-46 57 24-98 48-56-26Z" fill="#99816a"/><path d="M97 222v54l56 26v-54l-56-26Zm56 26 98-48v54l-98 48v-54Z" fill="#4a4742"/>
   <path d="m137 211 32-16 26 11-32 17-26-12Z" fill="#d8c69d"/>
   <path d="M280 34h80v6h-80z" fill="#c0b38d"/>
   <ellipse cx="321" cy="316" rx="44" ry="11" fill="#172028" opacity=".7"/>
   <path d="M305 237h31l10 54-26 12-25-12 10-54Z" fill="#b49a66"/><circle cx="320" cy="221" r="17" fill="#c8b8a0"/><path d="m309 296-6 23m26-23 7 23" stroke="#252e34" strokeWidth="10"/>
   <path d="M316 159h11l5 28h-21l5-28Z" fill="#151920"/><circle cx="322" cy="153" r="7" fill="#151920"/>
   <path d="m360 105 29 255H250l37-255" fill="#d3b184" fillOpacity=".025"/>
  </>:variant==='quest'?<>
   <path fill="#323943" d="M0 0h640v360H0z"/><path fill={paint('grid')} d="M0 0h640v360H0z"/>
   <ellipse cx="321" cy="288" rx="203" ry="34" fill="#1d2531" fillOpacity=".45"/>
   <path d="m136 249 194-81 184 81-193 84-185-84Z" fill="#7d8e86"/>
   <path d="m136 249 185 74v10l-185-74v-10Zm185 74 193-74v10l-193 74v-10Z" fill="#536c68"/>
   <path d="M270 101 370 60l51 23v170l-100 43-51-25V101Z" fill="#626470"/>
   <path d="m270 101 51 23 100-41-51-23-100 41Z" fill="#99939b"/>
   <path d="M321 124v172l100-43V83l-100 41Z" fill="#444c58"/>
   <path d="m335 134 70-29v138l-70 30V134Z" fill={paint('door')}/>
   <path d="m344 143 51-22v111l-51 22V143Z" stroke="#e4c9af" strokeOpacity=".4" strokeWidth="2"/>
   <path d="m390 184 6-3v12l-6 3v-12Z" fill="#efd29b"/>
   <path d="m233 259 74-31 48 20-75 32-47-21Z" fill="#b4aaa0"/>
   <path d="m223 270 76-31 48 20-76 33-48-22Z" fill="#999a93"/>
   <g transform="translate(180 130) rotate(-24)"><circle r="24" stroke="#ddc08b" strokeWidth="10"/><path d="M23 0h71v10H75v13H63V10H50V0" fill="#ddc08b"/><circle cx="-6" cy="-7" r="4" fill="#f4deb3"/></g>
   <path d="m456 234 23-10 18 8v22l-23 11-18-9v-22Z" fill="#79968b"/><path d="m447 225 25-11 26 12-25 11-26-12Z" fill="#a6b6a0"/>
   <path d="m213 80 4-13 4 13 13 4-13 4-4 13-4-13-13-4 13-4Z" fill="#c7bca4"/>
  </>:variant==='shooter'?<>
   <path fill="#444653" d="M0 0h640v360H0z"/><path d="M0 0h640v187H0z" fill="#a1a7af"/>
   <circle cx="475" cy="76" r="39" fill="#e3d1bb"/>
   <path d="m0 189 137-59 97 39 123-55 139 57 144-37v93H0v-38Z" fill="#737f87"/>
   <path d="M0 215 320 171l320 44v145H0V215Z" fill="#7d8487"/>
   <path d="M0 360 320 171l320 189M320 171v189M0 285h640M0 335h640" stroke="#a6a6a4" strokeOpacity=".25"/>
   <path d="m55 192 85-23 60 28v90l-87 30-58-31v-94Z" fill="#59656c"/><path d="m55 192 58 25 87-20-60-28-85 23Z" fill="#9c9f9b"/><path d="M113 217v100l87-30v-90l-87 20Z" fill="#424d57"/>
   <path d="m446 172 88-22 64 32v99l-85 27-67-36V172Z" fill="#59656c"/><path d="m446 172 67 31 85-21-64-32-88 22Z" fill="#a3a6a1"/><path d="M513 203v105l85-27v-99l-85 21Z" fill="#46525d"/>
   <path d="M264 175h112v65H264z" fill="#303b47"/><path d="M264 175h112v5H264z" fill="#c2a67f"/>
   <g transform="translate(322 230)"><ellipse cy="33" rx="26" ry="7" fill="#3d4651" fillOpacity=".5"/><path d="M-13-17h26l7 38-20 8-20-8 7-38Z" fill="#b58b83"/><circle cy="-25" r="13" fill="#e2d4bf"/><path d="m-8 22-4 13m20-13 4 13" stroke="#42414a" strokeWidth="8"/></g>
   <g stroke="#f3e6d0" strokeWidth="2" opacity=".65"><circle cx="320" cy="177" r="18"/><path d="M320 151v12m0 28v12m-26-26h12m28 0h12"/></g>
   <path d="m307 360 7-17h12l7 17m-20-47 3-13h8l3 13m-12-38 2-9h6l2 9" fill="#d3b88a"/>
  </>:<>
   <path fill="#383a49" d="M0 0h640v360H0z"/><path fill={paint('grid')} d="M0 0h640v360H0z"/>
   <ellipse cx="330" cy="294" rx="180" ry="26" fill="#252c3b" fillOpacity=".5"/>
   <path d="m222 83 203 25-13 185-203-25 13-185Z" fill="#222b3a"/>
   <path d="m235 96 175 22-11 158-176-21 12-159Z" fill="#4f5365"/>
   <path d="m249 117 143 17-2 28-143-17 2-28Z" fill="#767083"/>
   <path d="m248 128 44 5m8 1 18 2" stroke="#ded0c5" strokeWidth="4"/>
   <path d="m264 222 103 12-1 22-104-12 2-22Z" fill="#c2a3a8"/>
   <path d="m284 234 62 7" stroke="#504957" strokeWidth="3"/>
   <g transform="translate(320 191)"><ellipse rx="31" ry="35" transform="rotate(18)" fill="#9c7e5f"/><ellipse cx="-6" cy="-2" rx="29" ry="35" transform="rotate(18)" fill="#d9bd8c"/><ellipse cx="-6" cy="-2" rx="22" ry="28" transform="rotate(18)" stroke="#ae8b60" strokeWidth="2"/><path d="m-4-19-9 32m-4-30 24 3M-21 10l25 4" stroke="#f1d9aa" strokeWidth="4"/></g>
   <g transform="translate(466 102) rotate(24)"><ellipse rx="24" ry="29" fill="#ab865e"/><ellipse cx="-5" rx="23" ry="29" fill="#e1c599"/><path d="M-6-16v32m-10-23 21 1m-21 13h21" stroke="#aa8559" strokeWidth="3"/></g>
   <g transform="translate(177 240) rotate(-18)"><ellipse rx="22" ry="27" fill="#a07c57"/><ellipse cx="-4" rx="21" ry="27" fill="#d2b584"/><path d="M-4-14v28m-10-21h20m-20 14h20" stroke="#a07c57" strokeWidth="3"/></g>
   <path d="m432 225 5-14 5 14 14 5-14 5-5 14-5-14-14-5 14-5Zm-232-98 3-9 3 9 9 3-9 3-3 9-3-9-9-3 9-3Z" fill="#d6bfa2"/>
  </>}
 </svg>;
}

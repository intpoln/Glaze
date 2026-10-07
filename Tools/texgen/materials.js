// Glaze texture pack recipes for Pocket Texture Lab (window.PTL).
// Load in the lab page served by receiver.py:  eval(await (await fetch('/materials.js')).text())
// Then:  await GZ.build('Rock_Cliff')  or  await GZ.buildAll()
// Every material ends with 3 outputs: BaseColor (sRGB), Normal (DirectX, for UE), ORM (AO, Roughness, Metallic).

window.GZ = (() => {
  const hex = (h, a = 1) => [parseInt(h.slice(1, 3), 16) / 255, parseInt(h.slice(3, 5), 16) / 255, parseInt(h.slice(5, 7), 16) / 255, a];
  const stops = list => list.map(([p, h]) => ({ p, c: hex(h) }));

  // Small graph builder: every helper returns a node id.
  const n = (type, params = {}) => PTL.addNode(type, { params });
  const c = (from, to, toPort = 0, fromPort = 0) => { PTL.connect(from, fromPort, to, toPort); return to; };
  const lv = (input, params) => c(input, n('levels', params));
  const inv = input => c(input, n('invert', {}));
  const ramp = (input, list) => c(input, n('ramp', { stops: stops(list) }));
  const blend = (a, b, mode, opacity = 1, mask = null) => {
    const id = n('blend', { mode, opacity });
    c(a, id, 0); c(b, id, 1);
    if (mask) c(mask, id, 2);
    return id;
  };
  const warp = (input, map, params) => { const id = n('warp', params); c(input, id, 0); c(map, id, 1); return id; };
  const blur = (input, sigma) => c(input, n('gaussian', { sigma }));
  const noise = params => n('noise', params);

  // Normal + ORM + 3 outputs. rough/metal: node id or number. ao: node id, or derived from the height.
  function finish(name, { height, albedo, rough, metal = 0, ao = null, strength = 4, nblur = 0.5, aoAmount = 0.45 }) {
    const nrm = c(height, n('normal', { strength, blur: nblur, convention: 'dx' }));
    const aoNode = ao ?? lv(blur(height, 3), { outBlack: 1 - aoAmount });
    const orm = n('combine', { preset: 'orm' });
    c(aoNode, orm, 0);
    if (typeof rough === 'number') PTL.setParams(orm, { gSrc: 'C', gVal: rough }); else { c(rough, orm, 1); PTL.setParams(orm, { gSrc: 'R' }); }
    if (typeof metal === 'number') PTL.setParams(orm, { bSrc: 'C', bVal: metal }); else { c(metal, orm, 2); PTL.setParams(orm, { bSrc: 'R' }); }
    const outBC = c(albedo, n('output', { usage: 'basecolor', filename: `T_${name}_BC` }));
    c(nrm, n('output', { usage: 'normal', filename: `T_${name}_N` }));
    c(orm, n('output', { usage: 'orm', filename: `T_${name}_ORM` }));
    PTL.setActiveOutput(outBC);
  }

  // ---- Recipes. Built-in templates are loaded by index; the rest are graphs built here. ----
  const recipes = {
    Brick_Red: { example: 3 },
    Asphalt_Cracked: { example: 4 },
    Cobblestone: { example: 5 },
    Wood_Planks: { example: 6 },
    Metal_Painted: { example: 7 },

    Rock_Cliff(name) {
      const ridged = noise({ fractal: 'ridged', scale: 3, octaves: 7, seed: 11 });
      const cracks = c(n('voronoi', { mode: 'crackle', scale: 6, seed: 4 }), n('levels', { inWhite: 0.35 }));
      const wmap = noise({ scale: 4, seed: 23 });
      const height = warp(blend(ridged, cracks, 'multiply', 0.8), wmap, { mode: 'gradient', intensity: 0.04 });
      const tint = noise({ scale: 2, octaves: 3, seed: 5 });
      const albedo = blend(ramp(height, [[0, '#2e2b28'], [0.45, '#5f5a53'], [0.8, '#8a847a'], [1, '#a59e92']]),
        ramp(tint, [[0, '#7a6e60'], [1, '#c8c2b8']]), 'multiply', 0.45);
      finish(name, { height, albedo, rough: lv(height, { outBlack: 0.75, outWhite: 0.92 }), strength: 7 });
    },

    Concrete(name) {
      const base = noise({ scale: 4, fractal: 'billow', octaves: 6, seed: 3 });
      const pores = inv(n('splatter', { pattern: 'gauss', count: 2000, size: 0.012, sizeRand: 0.7, valRand: 0.5, seed: 9 }));
      const height = blend(lv(base, { outBlack: 0.35, outWhite: 0.65 }), pores, 'multiply', 0.7);
      const stains = lv(noise({ scale: 3, warp: 0.4, seed: 17 }), { outBlack: 0.78 });
      const albedo = blend(ramp(height, [[0, '#6e6c68'], [0.6, '#9a9893'], [1, '#b3b1ab']]), stains, 'multiply', 0.6);
      finish(name, { height, albedo, rough: lv(height, { outBlack: 0.82, outWhite: 0.95 }), strength: 2 });
    },

    Tiles_Ceramic(name) {
      const tiles = n('tiler', { pattern: 'square', countX: 4, countY: 4, rowOffset: 0, sizeX: 0.95, sizeY: 0.95, bevel: 0.05, valRand: 0.1, seed: 2 });
      const grout = inv(lv(tiles, { inWhite: 0.05 }));
      const height = blend(tiles, noise({ scale: 16, octaves: 3, seed: 7 }), 'multiply', 0.08);
      const tileColor = blend(ramp(c(tiles, n('levels', {}), 0, 1), [[0, '#d9e3e8'], [1, '#eef3f5']]),
        n('constant', { mode: 'color', color: hex('#7d7a74') }), 'mix', 1, grout);
      finish(name, { height, albedo: tileColor, rough: blend(n('constant', { value: 0.18 }), n('constant', { value: 0.85 }), 'mix', 1, grout), strength: 3, aoAmount: 0.3 });
    },

    Marble(name) {
      const veins = noise({ fractal: 'ridged', scale: 2, octaves: 6, warp: 0.7, warpScale: 2, seed: 8 });
      const v = lv(veins, { inBlack: 0.55, inWhite: 1, gamma: 1.6 });
      const albedo = ramp(v, [[0, '#ece9e4'], [0.6, '#d6d2cb'], [0.9, '#8f8b86'], [1, '#5d5a57']]);
      const height = lv(v, { outWhite: 0.15 });
      finish(name, { height, albedo, rough: lv(v, { outBlack: 0.12, outWhite: 0.28 }), strength: 0.6, aoAmount: 0.05 });
    },

    Dirt_Ground(name) {
      const soil = noise({ fractal: 'billow', scale: 6, octaves: 6, seed: 12 });
      const pebbles = n('splatter', { pattern: 'disc', count: 500, size: 0.025, sizeRand: 0.7, bevel: 0.5, valRand: 0.6, seed: 14 });
      const height = blend(lv(soil, { outWhite: 0.6 }), pebbles, 'max');
      const wet = lv(noise({ scale: 3, seed: 31 }), { outBlack: 0.7 });
      const albedo = blend(ramp(height, [[0, '#3b2a1c'], [0.5, '#5e4530'], [0.75, '#7c6249'], [1, '#9b8a78']]), wet, 'multiply', 0.5);
      finish(name, { height, albedo, rough: lv(height, { outBlack: 0.8, outWhite: 0.95 }), strength: 5 });
    },

    Fabric(name) {
      const warpY = n('waves', { countX: 48, countY: 0, shape: 'sine' });
      const warpX = n('waves', { countX: 0, countY: 48, shape: 'sine' });
      const weave = warp(blend(warpY, warpX, 'multiply'), noise({ scale: 32, seed: 4 }), { mode: 'gradient', intensity: 0.002 });
      const fuzz = noise({ type: 'value', scale: 64, octaves: 3, seed: 6 });
      const height = blend(weave, fuzz, 'multiply', 0.3);
      const albedo = blend(ramp(height, [[0, '#2b3d5c'], [1, '#4f6a94']]), noise({ scale: 4, seed: 2 }), 'multiply', 0.15);
      finish(name, { height, albedo, rough: 0.9, strength: 2.5, aoAmount: 0.35 });
    },

    Metal_Rusty(name) {
      const brushed = noise({ type: 'value', scale: 4, stretch: 48, octaves: 5, seed: 3 });
      const rustMask = lv(noise({ scale: 4, warp: 0.5, octaves: 6, seed: 19 }), { inBlack: 0.48, inWhite: 0.6 });
      const rustTex = noise({ fractal: 'billow', scale: 16, octaves: 5, seed: 21 });
      const albedo = blend(ramp(brushed, [[0, '#6d7074'], [1, '#9ea2a6']]),
        ramp(rustTex, [[0, '#4a2310'], [0.5, '#8a4519'], [1, '#b8692c']]), 'mix', 1, rustMask);
      const height = blend(lv(brushed, { outWhite: 0.2 }), blend(rustTex, rustMask, 'multiply'), 'max');
      finish(name, { height, albedo, rough: blend(n('constant', { value: 0.35 }), n('constant', { value: 0.9 }), 'mix', 1, rustMask), metal: inv(rustMask), strength: 3, aoAmount: 0.25 });
    },

    Metal_Brushed(name) {
      const brushed = noise({ type: 'value', scale: 4, stretch: 96, octaves: 6, seed: 5 });
      const scratches = n('splatter', { pattern: 'square', aspect: 0.01, size: 0.3, count: 60, rotRand: 15, bevel: 0, valRand: 0.6, seed: 8 });
      const height = blend(lv(brushed, { outWhite: 0.3 }), scratches, 'max', 0.4);
      const albedo = ramp(brushed, [[0, '#8e9297'], [1, '#c4c8cc']]);
      finish(name, { height, albedo, rough: lv(brushed, { outBlack: 0.28, outWhite: 0.42 }), metal: 1, strength: 0.8, aoAmount: 0.05 });
    },

    Plaster(name) {
      const big = noise({ scale: 4, octaves: 6, seed: 13 });
      const trowel = noise({ type: 'flow', scale: 8, octaves: 4, seed: 15 });
      const height = blend(lv(big, { outBlack: 0.4, outWhite: 0.6 }), trowel, 'overlay', 0.5);
      const albedo = blend(ramp(height, [[0, '#cbbfa8'], [1, '#e6dccb']]), lv(noise({ scale: 2, seed: 41 }), { outBlack: 0.85 }), 'multiply', 0.7);
      finish(name, { height, albedo, rough: 0.85, strength: 1.5, aoAmount: 0.2 });
    },

    Sand(name) {
      const dunes = c(n('waves', { countY: 12, shape: 'sine', distort: 6 }), n('levels', {}));
      const distort = noise({ scale: 3, octaves: 4, seed: 9 });
      const ripples = warp(dunes, distort, { mode: 'gradient', intensity: 0.08 });
      const grain = noise({ type: 'white', grain: 1, octaves: 1, seed: 2 });
      const height = blend(lv(ripples, { outWhite: 0.6 }), grain, 'add', 0.12);
      const albedo = blend(ramp(height, [[0, '#b89564'], [1, '#e2c895']]), grain, 'multiply', 0.1);
      finish(name, { height, albedo, rough: 0.9, strength: 2.5, aoAmount: 0.2 });
    },

    Grass_Stylized(name) {
      const blades = n('splatter', { pattern: 'gauss', count: 2500, size: 0.06, aspect: 0.15, sizeRand: 0.6, rotation: 90, rotRand: 30, valRand: 0.8, blend: 'max', seed: 4 });
      const clumps = noise({ scale: 4, octaves: 4, seed: 27 });
      const height = blend(blades, lv(clumps, { outWhite: 0.25 }), 'max');
      const albedo = blend(ramp(blades, [[0, '#36571f'], [0.4, '#4f7d2b'], [0.8, '#6f9e3a'], [1, '#a8cf5e']]),
        ramp(clumps, [[0, '#9aa86a'], [1, '#ffffff']]), 'multiply', 0.5);
      finish(name, { height, albedo, rough: 0.8, strength: 3, aoAmount: 0.5 });
    },

    Flagstone(name) {
      const wmap = noise({ scale: 4, seed: 33 });
      const border = warp(n('voronoi', { mode: 'border', scale: 5, randomness: 0.8, seed: 6, edgeSmooth: 0.6 }), wmap, { mode: 'gradient', intensity: 0.03 });
      const cells = warp(n('voronoi', { mode: 'cell', scale: 5, randomness: 0.8, seed: 6 }), wmap, { mode: 'gradient', intensity: 0.03 });
      const stone = lv(blur(border, 8), { inBlack: 0.02, inWhite: 0.5, gamma: 1.6 });
      const height = blend(stone, noise({ scale: 16, octaves: 5, seed: 8 }), 'multiply', 0.25);
      const gap = inv(lv(border, { inWhite: 0.05 }));
      const albedo = blend(ramp(cells, [[0, '#8b7d68'], [0.5, '#a89679'], [1, '#c2b293']]),
        n('constant', { mode: 'color', color: hex('#4a4136') }), 'mix', 1, gap);
      finish(name, { height, albedo, rough: 0.85, strength: 3 });
    },

    Wood_Grain(name) {
      const knots = n('splatter', { pattern: 'gauss', count: 4, size: 0.12, aspect: 0.5, seed: 12 });
      const dist = blend(noise({ scale: 1, stretch: 4, seed: 3 }), knots, 'add', 0.6);
      const rings = c(dist, n('waves', { countY: 40, distort: 10, shape: 'sine' }));
      const lines = lv(rings, { inBlack: 0.6, inWhite: 1 });
      const fibers = noise({ type: 'value', scale: 4, stretch: 32, seed: 7 });
      const height = blend(inv(lines), fibers, 'multiply', 0.3);
      const albedo = blend(ramp(lines, [[0, '#b07d4b'], [0.7, '#93643a'], [1, '#5e3b1f']]), fibers, 'multiply', 0.2);
      finish(name, { height, albedo, rough: 0.7, strength: 1.5, aoAmount: 0.15 });
    },
  };

  async function post(path, body) {
    const r = await fetch('/save?path=' + encodeURIComponent(path), { method: 'POST', body });
    if (!r.ok) throw new Error('save ' + path + ' ' + r.status);
  }

  async function build(name) {
    const recipe = recipes[name];
    if (recipe.example !== undefined) {
      PTL.loadExample(recipe.example);
      for (const node of PTL.getGraph().nodes) if (node.type === 'normal') PTL.setParams(node.id, { convention: 'dx' });
    } else {
      PTL.newProject();
      PTL.batch(() => recipe(name));
      PTL.autoLayout();
    }
    await new Promise(r => setTimeout(r, 100));
    const errs = PTL.errors();
    if (Object.keys(errs).length) throw new Error(name + ': ' + JSON.stringify(errs));
  }

  const suffix = { basecolor: 'BC', normal: 'N', orm: 'ORM' };
  // <Name>/<2k|1k>/T_<Name>_<BC|N|ORM>.png plus the editable graph.
  async function exportMaterial(name, sizes = [2048, 1024]) {
    const outs = PTL.getGraph().nodes.filter(o => o.type === 'output');
    for (const s of sizes) for (const o of outs) {
      await post(`${name}/${s / 1024}k/T_${name}_${suffix[o.params.usage] || o.params.filename}.png`, await PTL.renderPNG(o.id, { size: s }));
    }
    await post(`${name}/${name}.graph.json`, JSON.stringify(PTL.saveProject()));
  }

  async function buildAll(names = Object.keys(recipes), sizes) {
    const done = [];
    for (const name of names) { await build(name); await exportMaterial(name, sizes); done.push(name); }
    return done;
  }

  return { recipes, build, exportMaterial, buildAll, hex };
})();

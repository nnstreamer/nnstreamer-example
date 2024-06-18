import globals from "globals";
import pluginJs from "@eslint/js";
import pluginPrettier from "eslint-plugin-prettier";

export default [
  {
    languageOptions: {
      globals: {
        ...globals.browser,
        tizen: true,
      },
    },
  },
  pluginJs.configs.recommended,
  {
    plugins: {
      prettier: pluginPrettier,
    },
    rules: pluginPrettier.configs.recommended.rules,
  },
];

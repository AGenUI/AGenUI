import { hapTasks } from '@ohos/hvigor-ohos-plugin';

// Note: test_fixtures are synced to rawfile/test_fixtures by tests/integration/platforms/harmony/harmony.sh
// before running ohosTest. Source: tests/integration/fixtures/test_fixtures/

export default {
  system: hapTasks, /* Built-in plugin of Hvigor. It cannot be modified. */
  plugins: []       /* Custom plugin to extend the functionality of Hvigor. */
}

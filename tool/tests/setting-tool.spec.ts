import { test, expect, type Page } from "@playwright/test";

async function open(page: Page, route = "bays") {
  await page.goto(`/#/${route}`);
  await expect(page.getByTestId("status-rules")).toBeVisible();
}

test.beforeEach(async ({ page }) => {
  await page.goto("/");
  await page.evaluate(() => window.localStorage.clear());
});

test("journey 1: multi-view navigation with protection-domain assertions", async ({ page }) => {
  await open(page, "bays");
  await expect(page.getByTestId("device-id")).toContainText("BUS1 FDR1 RELAY");
  const rows = page.getByTestId("bay-table").locator("tbody tr");
  await expect(rows).toHaveCount(4);
  await expect(page.getByTestId("bay-row-2")).toContainText("Transformer T1");
  await expect(page.getByTestId("bay-row-2")).toContainText("400:1");
  await expect(page.getByTestId("bay-row-1")).toContainText("toward line");

  await page.getByTestId("nav-overcurrent").click();
  await expect(page).toHaveURL(/#\/overcurrent/);
  await expect(page.getByTestId("curve-51p")).toHaveAttribute("d", /^M/);
  // IEEE C37.112 VI, TMS 0.5 at 3x: 0.5 * (19.61/(9-1) + 0.491) = 1.471 s
  await expect(page.getByTestId("t51p-at-3")).toHaveText("1.471");

  await page.getByTestId("nav-distance").click();
  await expect(page.getByTestId("z1-ohm")).toHaveText("2.10 ohm");
  await page.getByTestId("nav-frequency").click();
  await expect(page.locator("#f81uPickupHz")).toHaveValue("59.3");
  await expect(page.getByTestId("status-rules")).toContainText("all 29 inputs valid");
});

test("journey 2: invalid then valid entry on the 51 form, rule errors from shared math", async ({ page }) => {
  await open(page, "overcurrent");
  const tms = page.locator("#t51pTms");
  await tms.fill("0.01");
  await expect(page.getByTestId("error-R-51-002")).toContainText("0.05..15.0");
  await expect(tms).toHaveClass(/invalid/);
  await expect(page.getByTestId("status-rules")).toContainText("1 violation");
  await expect(page.getByTestId("nav-overcurrent").locator(".badge")).toHaveText("1");

  await page.locator("#i50pPickupA").fill("500");
  await expect(page.getByTestId("error-R-50-001")).toBeVisible();
  await expect(page.getByTestId("status-rules")).toContainText("2 violations");

  await tms.fill("0.8");
  await page.locator("#i50pPickupA").fill("6000");
  await expect(page.getByTestId("error-R-51-002")).toHaveCount(0);
  await expect(page.getByTestId("error-R-50-001")).toHaveCount(0);
  await expect(page.getByTestId("status-rules")).toContainText("all 29 inputs valid");
  await expect(page.getByTestId("t51p-at-3")).toHaveText("2.354");
});

test("journey 3: filter, sort and search over Renode scenario evidence", async ({ page }) => {
  await open(page, "realtime");
  const table = page.getByTestId("scenario-table");
  await expect(table.locator("tbody tr")).toHaveCount(8);
  await expect(page.getByTestId("task-table")).toContainText("protection");

  await page.getByTestId("scenario-filter").selectOption("trip");
  await expect(page.getByTestId("scenario-count")).toHaveText("4 of 8");
  await expect(table.locator("tbody tr")).toHaveCount(4);

  await page.getByTestId("scenario-search").fill("bus");
  await expect(table.locator("tbody tr")).toHaveCount(0);
  await page.getByTestId("scenario-filter").selectOption("all");
  await expect(table.locator("tbody tr")).toHaveCount(3);
  await expect(table).toContainText("bus_fault_internal");
  await expect(page.getByTestId("scenario-bus_fault_internal")).toContainText("no trip");

  await page.getByTestId("scenario-search").fill("");
  await page.getByTestId("sort-trip_ms").click();
  const first = table.locator("tbody tr").first();
  await expect(first).toContainText("fault_ag_zone1");
  await page.getByTestId("sort-trip_ms").click();
  await expect(table.locator("tbody tr").first()).not.toContainText("fault_ag_zone1");
});

test("journey 4: write drawer blocks on violations, then writes with side effects", async ({ page }) => {
  await open(page, "distance");
  await page.locator("#z2ReachPct").fill("70");
  await expect(page.getByTestId("error-R-21-003")).toBeVisible();
  await page.getByTestId("write-button").click();
  const drawer = page.getByTestId("write-drawer");
  await expect(drawer).toBeVisible();
  await expect(page.getByTestId("write-blocked")).toContainText("R-21-003");
  await expect(page.getByTestId("write-apply")).toBeDisabled();
  await page.getByTestId("write-cancel").click();
  await expect(drawer).toHaveCount(0);

  await page.locator("#z2ReachPct").fill("125");
  await expect(page.getByTestId("status-modified")).toHaveText("Modified - not written");
  await page.getByTestId("write-button").click();
  await expect(page.getByTestId("write-ready")).toBeVisible();
  await expect(page.getByTestId("settings-file")).toContainText("21_Z2_REACH_PCT = 125");
  await page.getByTestId("write-confirm").check();
  await page.getByTestId("write-apply").click();
  await expect(page.getByTestId("write-drawer")).toHaveCount(0);
  await expect(page.getByTestId("status-modified")).toHaveText("In sync with relay");
  await expect(page.getByTestId("status-last-write")).toContainText("Last write:");
});

test("journey 5: golden path keeps state across views and a reload", async ({ page }) => {
  await open(page, "bays");
  await page.getByTestId("bay-row-3").click();
  await page.locator("#bay-ct-primary").fill("0");
  await expect(page.getByTestId("error-R-CT-001")).toBeVisible();
  await expect(page.getByTestId("bay-row-3")).toContainText("R-CT-001");
  await page.locator("#bay-ct-primary").fill("1000");
  await expect(page.getByTestId("bay-row-3")).toContainText("200:1");

  await page.getByTestId("nav-overcurrent").click();
  await page.locator("#t51pCurve").selectOption("IEC_SI");
  await page.getByTestId("nav-bays").click();
  await expect(page.getByTestId("bay-row-3")).toContainText("1000");
  await page.reload();
  await expect(page.getByTestId("bay-row-3")).toContainText("1000");
  await page.getByTestId("nav-overcurrent").click();
  await expect(page.locator("#t51pCurve")).toHaveValue("IEC_SI");

  await page.getByTestId("write-button").click();
  await expect(page.getByTestId("settings-file")).toContainText("BAY3_CT_PRIMARY_A = 1000");
  await expect(page.getByTestId("settings-file")).toContainText("51P_CURVE = IEC_SI");
  await page.getByTestId("write-confirm").check();
  await page.getByTestId("write-apply").click();
  await expect(page.getByTestId("status-modified")).toHaveText("In sync with relay");
});

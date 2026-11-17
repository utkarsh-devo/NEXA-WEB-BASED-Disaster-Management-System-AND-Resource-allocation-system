# Migrations

`schema/schema.sql` is the baseline (v1). Put later changes here as ordered, idempotent scripts, e.g.
`002_add_resource_fuel_level.sql`, and apply them in order with `mysql -u root -p nexa < file.sql`.
Never edit an applied migration — add a new one.
